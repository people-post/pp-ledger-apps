// Development only:
//   pp-admin-devfixture <home> <passphrase>
//     Makes <home>/vault (cheap KDF) holding a reserve and an admin key and an address book;
//     <home>/transfer.json: a reserve transfer, 2 of 3, signed by Alice; <home>/transfer.bob.json:
//     the same, signed by Bob (a copy to merge); <home>/genesis.json: a genesis plan (pp-ledger's
//     builder) the vault's admin key is one of three admins of, 2 to sign.
//   pp-admin-devfixture import <home> <passphrase> <role> <name>.key…
//     Adds the given pp-client keygen keys (each with its .pub) to <home>/vault, made (cheap KDF)
//     if missing.
//   pp-admin-devfixture card <home> <passphrase> <role> <name> <name>.pub
//     Adds a key card to <home>/vault's address book.
//   pp-admin-devfixture verify <operation file> <signatures>
//     Exit 0 when the file holds exactly that many valid signatures (and no invalid one).
#include "core/AddressBook.h"
#include "core/Vault.h"
#include "chain/Genesis.h"
#include "client/AccountIds.h"
#include "operation/Operation.h"
#include "lib/common/Utilities.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <filesystem>
#include <iostream>

using namespace admin;
namespace op = pp::operation;

int main(int argc, char **argv) {
  if (argc == 4 && std::string(argv[1]) == "verify") {
    auto operation = op::load(argv[2]);
    if (!operation) {
      std::cerr << operation.error().message << "\n";
      return 1;
    }
    const auto check = op::check(operation.value());
    std::cout << check.signedBy.size() << " valid, " << check.invalid << " invalid\n";
    return check.signedBy.size() == static_cast<size_t>(std::atoi(argv[3])) && check.invalid == 0 ? 0 : 1;
  }
  if (argc >= 6 && std::string(argv[1]) == "import") {
    const std::filesystem::path home = argv[2];
    const auto role = roleFromName(argv[4]);
    if (!role) {
      std::cerr << "unknown role " << argv[4] << "\n";
      return 2;
    }
    Vault vault((home / "vault").string(), KdfLimits::minimum());
    if (auto opened = vault.exists() ? vault.unlock(argv[3]) : vault.create(argv[3]); !opened) {
      std::cerr << opened.error().message << "\n";
      return 1;
    }
    for (int i = 5; i < argc; ++i) {
      const std::filesystem::path keyPath = argv[i];
      auto privateKey = pp::utl::readPrivateKey(keyPath.string(), "");
      std::ifstream pubFile(std::filesystem::path(keyPath).replace_extension(".pub"));
      std::string pubHex;
      pubFile >> pubHex;
      if (!privateKey) {
        std::cerr << keyPath << ": " << privateKey.error().message << "\n";
        return 1;
      }
      auto key = vault.importKey(keyPath.stem().string(), *role, privateKey.value(), pp::utl::hexDecode(pubHex));
      if (!key) {
        std::cerr << keyPath << ": " << key.error().message << "\n";
        return 1;
      }
    }
    return 0;
  }
  if (argc == 7 && std::string(argv[1]) == "card") {
    const auto role = roleFromName(argv[4]);
    Vault vault((std::filesystem::path(argv[2]) / "vault").string(), KdfLimits::minimum());
    std::ifstream pubFile(argv[6]);
    std::string pubHex;
    pubFile >> pubHex;
    if (!role || !vault.unlock(argv[3])) {
      std::cerr << "bad role, or the vault does not unlock\n";
      return 1;
    }
    auto book = AddressBook::load(vault);
    if (!book) {
      std::cerr << book.error().message << "\n";
      return 1;
    }
    book.value().put({argv[5], *role, pp::utl::hexDecode(pubHex)});
    if (auto saved = book.value().save(vault); !saved) {
      std::cerr << saved.error().message << "\n";
      return 1;
    }
    return 0;
  }
  if (argc != 3) {
    std::cerr << "usage: pp-admin-devfixture <home> <passphrase>\n";
    return 2;
  }
  const std::filesystem::path home = argv[1];
  Vault vault((home / "vault").string(), KdfLimits::minimum());
  if (auto created = vault.create(argv[2]); !created) {
    std::cerr << created.error().message << "\n";
    return 1;
  }
  auto mine = vault.generateKey("Hongwei — reserve", Role::Reserve).value();
  auto myAdmin = vault.generateKey("Hongwei — admin", Role::Admin).value();
  const auto alice = pp::utl::mlDsaGenerate().value();
  const auto bob = pp::utl::mlDsaGenerate().value();
  AddressBook book;
  book.put({"Alice Chen", Role::Reserve, alice.publicKey});
  book.put({"Bob Okafor", Role::Reserve, bob.publicKey});
  if (auto saved = book.save(vault); !saved) {
    std::cerr << saved.error().message << "\n";
    return 1;
  }
  const int64_t now =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  op::Common common;
  common.networkId = "pp-mainnet";
  common.genesisHash = std::string(32, '\x42');
  common.signers.account = pp::AccountIds::ID_RESERVE;
  common.signers.minSignatures = 2;
  common.signers.publicKeys = {alice.publicKey, mine.publicKey, bob.publicKey};
  common.window = op::windowFrom(now, 86400);
  common.notes = "Monthly top-up of the registrar, as agreed on Monday.";
  common.createdAt = now;
  const auto unsignedTransfer = op::transfer(common, pp::AccountIds::ID_REGISTRAR, 5'000'000, 1).value();
  auto transfer = unsignedTransfer;
  op::sign(transfer, alice.privateKey).value();
  auto bobsCopy = unsignedTransfer;
  op::sign(bobsCopy, bob.privateKey).value();

  // A genesis plan, as pp-beacon --plan-genesis makes it.
  auto key = [] { return pp::utl::mlDsaGenerate().value().publicKey; };
  pp::chain_genesis::Plan plan;
  plan.config.networkId = "pp-mainnet";
  plan.config.genesisTime = now + 86400;
  plan.config.slotDuration = 7;
  plan.config.slotsPerEpoch = 1800;
  plan.config.maxTransactionsPerBlock = 4096;
  plan.config.maxBlockBytes = 4u << 20;
  plan.config.maxCustomMetaSize = 4096;
  plan.config.minFeeCoefficients = {1, 1, 0};
  plan.config.maxValidationTimespanSeconds = 86400;
  plan.config.maxIssuancePerEpoch = 1'000'000;
  plan.config.newAccountFee = 10;
  plan.config.minerMinStake = 1000;
  plan.config.minerRegistrationFee = 10;
  plan.keys.genesis = {{alice.publicKey, myAdmin.publicKey, bob.publicKey}, 2};
  plan.keys.reserve = {{mine.publicKey}, 0};
  plan.keys.registrar = {{key()}, 0};
  plan.keys.fee = {{key()}, 0};
  plan.keys.recycle = {{key()}, 0};
  plan.miners = {{pp::AccountIds::ID_FIRST_ISSUED, {key()}, 1, 10'000}};
  plan.authority.recovery.publicKeys = {key(), key(), key()};
  plan.authority.recovery.minSignatures = 2;
  plan.authority.recovery.delayEpochs = 200;
  plan.authority.beacon.generation = 1;
  plan.authority.beacon.publicKey = key();
  auto records = pp::chain_genesis::records(plan);
  if (!records) {
    std::cerr << records.error().message << "\n";
    return 1;
  }
  auto genesis = op::genesis(records.value(), "The launch plan, as agreed.", now).value();
  op::sign(genesis, alice.privateKey).value();

  for (const auto &[name, operation] : {std::pair<const char *, const op::Operation *>{"transfer.json", &transfer},
                                        {"transfer.bob.json", &bobsCopy},
                                        {"genesis.json", &genesis}}) {
    if (auto saved = op::save(*operation, (home / name).string(), true); !saved) {
      std::cerr << saved.error().message << "\n";
      return 1;
    }
  }
  std::cout << home.string() << "\n";
  return 0;
}
