#include "fod/vendor/fod_vendor_action.hpp"

namespace fod::vendor {
namespace {

bool DisplayResultSucceeded(const FodSysfsResult& result) noexcept {
    return result.status == FodSysfsStatus::kApplied ||
           result.status == FodSysfsStatus::kAlreadyApplied;
}

void RecordOperation(FodVendorActionResult& result, FodVendorOperation operation,
                     bool succeeded) noexcept {
    if (result.operations_attempted < static_cast<int>(result.operations.size())) {
        result.operations[static_cast<std::size_t>(result.operations_attempted)] = operation;
    }
    ++result.operations_attempted;
    if (succeeded) {
        ++result.operations_succeeded;
    } else {
        result.all_succeeded = false;
    }
}

}  // namespace

FodVendorActionDispatcher::FodVendorActionDispatcher(
    FodVendorActionBackend& backend) noexcept
    : backend_(backend) {}

FodVendorActionResult FodVendorActionDispatcher::Dispatch(
    binder::FingerprintTransactionAction action) noexcept {
    FodVendorActionResult result;
    result.action = action;

    switch (action) {
        case binder::FingerprintTransactionAction::kPointerDown: {
            const FodSysfsResult display =
                backend_.ApplyDisplayState(FodDisplayState::kFingerDown);
            RecordOperation(result, FodVendorOperation::kDisplayFingerDown,
                            DisplayResultSucceeded(display));
            const AidlIntTransactionResult fingerprint = backend_.SetFingerprintDown(true);
            RecordOperation(result, FodVendorOperation::kFingerprintDown, fingerprint.ok);
            return result;
        }
        case binder::FingerprintTransactionAction::kPointerUp: {
            const FodSysfsResult display =
                backend_.ApplyDisplayState(FodDisplayState::kFingerUp);
            RecordOperation(result, FodVendorOperation::kDisplayFingerUp,
                            DisplayResultSucceeded(display));
            const AidlIntTransactionResult fingerprint = backend_.SetFingerprintDown(false);
            RecordOperation(result, FodVendorOperation::kFingerprintUp, fingerprint.ok);
            return result;
        }
        case binder::FingerprintTransactionAction::kStartHal: {
            const AidlIntTransactionResult touch = backend_.SetTouchMode(true);
            RecordOperation(result, FodVendorOperation::kTouchModeOn, touch.ok);
            return result;
        }
        case binder::FingerprintTransactionAction::kSessionClose: {
            const AidlIntTransactionResult touch = backend_.SetTouchMode(false);
            RecordOperation(result, FodVendorOperation::kTouchModeOff, touch.ok);
            const FodSysfsResult display = backend_.ApplyDisplayState(FodDisplayState::kFingerUp);
            RecordOperation(result, FodVendorOperation::kDisplayFingerUp,
                            DisplayResultSucceeded(display));
            return result;
        }
        case binder::FingerprintTransactionAction::kNone:
        case binder::FingerprintTransactionAction::kUdfpsUiEvent:
            return result;
    }

    result.all_succeeded = false;
    return result;
}

}  // namespace fod::vendor
