#pragma once
// Operations as the apps handle them (DESIGN.md § How an operation travels): opened for review —
// what it does recomputed from its bytes, who has signed — signed with a vault key, merged.
// The file format and its rules are pp-ledger's (docs/contracts/OPERATION_FILE.md).

#include "core/AddressBook.h"
#include "core/Vault.h"
#include "operation/Operation.h"

#include <string>
#include <vector>

namespace apps {

/** An operation as a reviewer sees it. */
struct Review {
  pp::operation::Operation operation;
  /** Recomputed from the record (AD007). */
  pp::operation::Summary summary;
  pp::operation::SignatureCheck signatures;
};

/**
 * Read an operation file for review. A file carrying a signature that is no signer's is refused:
 * it was tampered with, or mixed up with another operation.
 */
pp::Roe<Review> openOperation(const std::string &path);

/**
 * Add the signatures of copies signed apart (`paths`, each opened like any file for review) to
 * `review` (AD002): only signatures may differ. The number of signers gained.
 */
pp::Roe<size_t> addCopies(Review &review, const std::vector<std::string> &paths);

/** The vault keys that may sign `operation` now: among its signers, not signed yet. */
std::vector<KeyInfo> keysToSign(const pp::operation::Operation &operation, const std::vector<KeyInfo> &vaultKeys);

/** Sign `operation` with vault key `id`. */
pp::Roe<void> signWithVault(const Vault &vault, const std::string &id, pp::operation::Operation &operation);

/**
 * A signer as people read it: the address book's name for the key, else the vault's label, else
 * its fingerprint; with the fingerprint, which is what people compare.
 */
std::string signerName(const std::string &publicKey, const AddressBook &book, const std::vector<KeyInfo> &vaultKeys);

/** Past its validity window at `now` (Unix seconds): it can only be made again. */
bool expired(const pp::operation::Summary &summary, int64_t now);

/** Where a signed copy goes by default: next to `path`, named for the key ("x.json" → "x.signed-abcd….json"). */
std::string signedCopyPath(const std::string &path, const std::string &publicKey);

} // namespace apps
