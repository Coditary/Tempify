#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tempify {

class TempifyError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct ReapplyOriginMismatchInfo {
    std::string lockfile_path;
    std::string origin_template_id;
    std::string origin_template_version;
    std::string requested_template_id;
    std::string requested_template_version;
};

struct ReapplyVersionTransitionInfo {
    std::string lockfile_path;
    std::string kind;
    std::string reason;
    std::string from_version;
    std::string to_version;
};

class ReapplyBlockedError : public TempifyError {
  public:
    ReapplyBlockedError(std::string message, std::vector<std::string> conflict_paths,
                        std::vector<std::string> review_paths,
                        std::optional<ReapplyOriginMismatchInfo> origin_mismatch = std::nullopt,
                        std::optional<ReapplyVersionTransitionInfo> version_transition = std::nullopt)
        : TempifyError(std::move(message)), conflict_paths_(std::move(conflict_paths)),
          review_paths_(std::move(review_paths)), origin_mismatch_(std::move(origin_mismatch)),
          version_transition_(std::move(version_transition)) {}

    const std::vector<std::string> &conflict_paths() const noexcept {
        return conflict_paths_;
    }

    const std::vector<std::string> &review_paths() const noexcept {
        return review_paths_;
    }

    const std::optional<ReapplyOriginMismatchInfo> &origin_mismatch() const noexcept {
        return origin_mismatch_;
    }

    const std::optional<ReapplyVersionTransitionInfo> &version_transition() const noexcept {
        return version_transition_;
    }

  private:
    std::vector<std::string> conflict_paths_;
    std::vector<std::string> review_paths_;
    std::optional<ReapplyOriginMismatchInfo> origin_mismatch_;
    std::optional<ReapplyVersionTransitionInfo> version_transition_;
};

} // namespace tempify
