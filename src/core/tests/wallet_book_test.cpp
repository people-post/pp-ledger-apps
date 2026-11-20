// The wallet's records: the network card it pins, the accounts it follows.
#include "core/WalletBook.h"
#include "TempDir.h"

#include <gtest/gtest.h>

using namespace apps;
using namespace apps::wallet;

namespace {

NetworkCard card() {
  NetworkCard c;
  c.networkId = "pp-main";
  c.genesisHash = std::string(32, '\x5a');
  c.nodes = {"/dns4/relay1.example/udp/8519/adp/1.0.0/p2p/QmA", "/ip4/10.0.0.2/udp/8519/adp/1.0.0/p2p/QmB"};
  c.notes = "published by the operators";
  return c;
}

} // namespace

TEST(WalletBookTest, ANetworkCardReadsBackAndRefusesWhatIsNotOne) {
  auto back = networkCardFromJson(networkCardToJson(card()));
  ASSERT_TRUE(back) << back.error().message;
  EXPECT_EQ(back.value(), card());
  EXPECT_EQ(hashForPeople(std::string(4, '\x5a') + std::string(4, '\x01')), "5a5a5a5a 01010101");

  std::string json = networkCardToJson(card());
  const auto at = json.find("5a5a");
  json.replace(at, 4, "");
  EXPECT_FALSE(networkCardFromJson(json)) << "a hash of 30 bytes";
  NetworkCard noNetwork = card();
  noNetwork.networkId.clear();
  EXPECT_FALSE(networkCardFromJson(networkCardToJson(noNetwork)));
  NetworkCard badNode = card();
  badNode.nodes.push_back("relay1.example:8519");
  EXPECT_FALSE(networkCardFromJson(networkCardToJson(badNode)));
  EXPECT_FALSE(networkCardFromJson("{\"format\": \"pp-ledger-operation\"}"));
}

TEST(WalletBookTest, TheVaultKeepsTheCardAndTheAccounts) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create("a long enough passphrase"));
  ASSERT_TRUE(vault.unlock("a long enough passphrase"));
  auto empty = WalletBook::load(vault);
  ASSERT_TRUE(empty) << empty.error().message;
  EXPECT_FALSE(empty.value().card);
  WalletBook book;
  book.card = card();
  book.accounts = {{(1ULL << 30) + 7, "savings"}, {(1ULL << 30) + 9, ""}};
  ASSERT_TRUE(book.save(vault));
  auto back = WalletBook::load(vault);
  ASSERT_TRUE(back) << back.error().message;
  EXPECT_EQ(back.value().card, book.card);
  EXPECT_EQ(back.value().accounts, book.accounts);
}
