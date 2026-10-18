// Development only:
//   pp-admin-devfixture <home> <passphrase>
//     Makes <home>/vault (cheap KDF) holding a reserve and an admin key and an address book, and
//     <home>/transfer.json: a reserve transfer, 2 of 3, signed by a third holder.
//   pp-admin-devfixture verify <operation file> <signatures>
//     Exit 0 when the file holds exactly that many valid signatures (and no invalid one).
#include "core/AddressBook.h"
#include "core/Vault.h"
#include "client/AccountIds.h"
#include "operation/Operation.h"
#include "lib/common/Utilities.h"

#include <chrono>
#include <cstdlib>
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
  vault.generateKey("Hongwei — admin", Role::Admin).value();
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
  auto transfer = op::transfer(common, pp::AccountIds::ID_REGISTRAR, 5'000'000, 1).value();
  op::sign(transfer, alice.privateKey).value();
  if (auto saved = op::save(transfer, (home / "transfer.json").string(), true); !saved) {
    std::cerr << saved.error().message << "\n";
    return 1;
  }
  std::cout << (home / "transfer.json").string() << "\n";
  return 0;
}
