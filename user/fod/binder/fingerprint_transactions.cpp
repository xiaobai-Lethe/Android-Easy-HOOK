#include "fod/binder/fingerprint_transactions.hpp"

namespace fod::binder {
namespace {

bool Contains(std::string_view value, std::string_view needle) noexcept {
    return !needle.empty() && value.find(needle) != std::string_view::npos;
}

bool IsFingerprintClientRole(core::ProcessRole role) noexcept {
    return role == core::ProcessRole::kSettings || role == core::ProcessRole::kSystemUi;
}

FingerprintTransactionAction SessionAction(int code) noexcept {
    switch (code) {
        case 3:
        case 4:
        case 15:
        case 16:
        case 17:
        case 18:
        case 21:
            return FingerprintTransactionAction::kStartHal;
        case 11:
            return FingerprintTransactionAction::kSessionClose;
        case 12:
        case 19:
            return FingerprintTransactionAction::kPointerDown;
        case 13:
        case 20:
        case 22:
            return FingerprintTransactionAction::kPointerUp;
        default:
            return FingerprintTransactionAction::kNone;
    }
}

FingerprintTransactionAction ServiceAction(int code) noexcept {
    switch (code) {
        case 34:
            return FingerprintTransactionAction::kPointerDown;
        case 35:
            return FingerprintTransactionAction::kPointerUp;
        case 36:
            return FingerprintTransactionAction::kUdfpsUiEvent;
        default:
            return FingerprintTransactionAction::kNone;
    }
}

}  // namespace

DescriptorKind FingerprintTransactions::ClassifyDescriptor(
    std::string_view descriptor) noexcept {
    if (Contains(descriptor, kSessionDescriptor)) {
        return DescriptorKind::kSession;
    }
    if (Contains(descriptor, kServiceDescriptor)) {
        return DescriptorKind::kService;
    }
    return DescriptorKind::kUnknown;
}

bool FingerprintTransactions::IsCandidateSessionCode(int code) noexcept {
    return code >= 1 && code <= 24;
}

bool FingerprintTransactions::IsCandidateServiceCode(int code) noexcept {
    return code >= 1 && code <= 48;
}

FingerprintTransactionDecision FingerprintTransactions::Classify(
    core::ProcessRole role, int code, std::string_view descriptor) noexcept {
    const DescriptorKind descriptor_kind = ClassifyDescriptor(descriptor);
    if (role == core::ProcessRole::kSystemServer &&
        descriptor_kind == DescriptorKind::kSession && IsCandidateSessionCode(code)) {
        return FingerprintTransactionDecision{
            .action = SessionAction(code),
        };
    }

    if (IsFingerprintClientRole(role) && descriptor_kind == DescriptorKind::kService &&
        IsCandidateServiceCode(code)) {
        return FingerprintTransactionDecision{
            .action = ServiceAction(code),
        };
    }

    return FingerprintTransactionDecision{};
}

}  // namespace fod::binder
