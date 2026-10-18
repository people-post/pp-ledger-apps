// Key cards and the address book: names for keys, exchanged as files, kept in the vault.
#include "core/AddressBook.h"
#include "core/KeyCard.h"
#include "TempDir.h"

#include "lib/common/Utilities.h"

#include <gtest/gtest.h>

using namespace apps;

TEST(KeyCardTest, ACardReadsBackAndAChangedOneIsRefused) {
  test::TempDir dir;
  KeyCard card{"Alice", Role::Recovery, pp::utl::mlDsaGenerate().value().publicKey};
  ASSERT_TRUE(saveKeyCard(card, dir / "alice.card.json"));
  EXPECT_FALSE(saveKeyCard(card, dir / "alice.card.json"));
  auto back = loadKeyCard(dir / "alice.card.json");
  ASSERT_TRUE(back) << back.error().message;
  EXPECT_EQ(back.value(), card);

  // Another key under Alice's fingerprint: refused.
  std::string json = toJson(card);
  const std::string otherHex = pp::utl::hexEncode(pp::utl::mlDsaGenerate().value().publicKey);
  json.replace(json.find(pp::utl::hexEncode(card.publicKey)), otherHex.size(), otherHex);
  EXPECT_FALSE(keyCardFromJson(json));
  EXPECT_FALSE(keyCardFromJson("{\"format\": \"something else\"}"));
}

TEST(KeyCardTest, TheAddressBookKeepsNamesInTheVault) {
  test::TempDir dir;
  Vault vault(dir / "vault", KdfLimits::minimum());
  ASSERT_TRUE(vault.create("correct horse battery"));
  const std::string alice = pp::utl::mlDsaGenerate().value().publicKey;
  const std::string bob = pp::utl::mlDsaGenerate().value().publicKey;
  auto book = AddressBook::load(vault);
  ASSERT_TRUE(book);
  EXPECT_TRUE(book.value().cards().empty());
  book.value().put({"Alice", Role::Admin, alice});
  book.value().put({"Bob", Role::Reserve, bob});
  book.value().put({"Alice B.", Role::Admin, alice}); // same key: renamed
  ASSERT_TRUE(book.value().save(vault));

  auto again = AddressBook::load(vault);
  ASSERT_TRUE(again);
  ASSERT_EQ(again.value().cards().size(), 2u);
  EXPECT_EQ(again.value().find(alice)->name, "Alice B.");
  EXPECT_EQ(again.value().find(bob)->role, Role::Reserve);
  EXPECT_TRUE(again.value().remove(bob));
  EXPECT_FALSE(again.value().find(bob));
}
