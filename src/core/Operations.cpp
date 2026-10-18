#include "core/Operations.h"

#include <algorithm>
#include <filesystem>

namespace admin {

namespace op = pp::operation;

pp::Roe<Review> openOperation(const std::string &path) {
  auto operation = op::load(path);
  if (!operation) {
    return operation.error();
  }
  Review review;
  review.signatures = op::check(operation.value());
  if (review.signatures.invalid > 0) {
    return pp::Error(path + " carries " + std::to_string(review.signatures.invalid) +
                     " signature(s) by no signer: it was changed, or mixed with another operation. Ask for a fresh copy");
  }
  auto summary = op::summarize(operation.value());
  if (!summary) {
    return pp::Error(path + ": " + summary.error().message);
  }
  review.summary = std::move(summary.value());
  review.operation = std::move(operation.value());
  return review;
}

pp::Roe<size_t> addCopies(Review &review, const std::vector<std::string> &paths) {
  std::vector<op::Operation> copies{review.operation};
  for (const auto &path : paths) {
    auto copy = openOperation(path);
    if (!copy) {
      return copy.error();
    }
    copies.push_back(std::move(copy.value().operation));
  }
  auto merged = op::merge(copies);
  if (!merged) {
    return pp::Error(merged.error().message + ". A copy was made from another operation file");
  }
  const size_t before = review.signatures.signedBy.size();
  review.operation = std::move(merged.value());
  review.signatures = op::check(review.operation);
  return review.signatures.signedBy.size() - before;
}

std::vector<KeyInfo> keysToSign(const op::Operation &operation, const std::vector<KeyInfo> &vaultKeys) {
  const auto signedBy = op::check(operation).signedBy;
  std::vector<KeyInfo> out;
  for (const auto &key : vaultKeys) {
    const auto &signers = operation.signers.publicKeys;
    if (std::find(signers.begin(), signers.end(), key.publicKey) != signers.end() &&
        std::find(signedBy.begin(), signedBy.end(), key.publicKey) == signedBy.end()) {
      out.push_back(key);
    }
  }
  return out;
}

pp::Roe<void> signWithVault(const Vault &vault, const std::string &id, op::Operation &operation) {
  return vault.withPrivateKey(id, [&](const std::string &privateKey) -> pp::Roe<void> {
    auto signedBy = op::sign(operation, privateKey);
    if (!signedBy) {
      return signedBy.error();
    }
    return {};
  });
}

std::string signerName(const std::string &publicKey, const AddressBook &book, const std::vector<KeyInfo> &vaultKeys) {
  const std::string fingerprint = op::fingerprint(publicKey);
  if (auto card = book.find(publicKey)) {
    return card->name + " (" + roleTitle(card->role) + ", " + fingerprint + ")";
  }
  for (const auto &key : vaultKeys) {
    if (key.publicKey == publicKey) {
      return key.label + " (your key, " + fingerprint + ")";
    }
  }
  return fingerprint;
}

bool expired(const op::Summary &summary, int64_t now) { return summary.validUntil != 0 && now > summary.validUntil; }

std::string signedCopyPath(const std::string &path, const std::string &publicKey) {
  std::filesystem::path p(path);
  std::string fingerprint = op::fingerprint(publicKey);
  std::erase(fingerprint, '-');
  const std::string stem = p.stem().string();
  return (p.parent_path() / (stem + ".signed-" + fingerprint.substr(0, 8) + p.extension().string())).string();
}

} // namespace admin
