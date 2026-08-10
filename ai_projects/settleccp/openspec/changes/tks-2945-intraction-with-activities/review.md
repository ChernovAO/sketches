# Review Findings

## 1. Critical: Pre-launch cancellation cannot be performed and would be overwritten

`cancel_activity` requires an existing activity record, but records are only initialized immediately before starting the child workflow. The status spec explicitly says activities are absent while the slot is `PENDING`.

Additionally, `_init_activity_states` recreates every record as `PENDING`, so any earlier cancellation would be lost.

Initialize activities when the slot is created and make initialization preserve existing terminal records.

References:

- `specs/activity-cancel/spec.md:13-16`
- `specs/activity-status-query/spec.md:77-89`
- `tasks.md:56-57`
- `tasks.md:77`

## 2. Critical: The inclusion filter cannot encode pre-launch cancellation reliably

An empty `activities_to_run` means "run everything." Removing a cancelled activity from the empty tuple still produces an empty tuple, so automatic and full launches execute the cancelled activity.

Even constructing "all except cancelled" fails when every activity is cancelled because the resulting empty tuple again means "run everything."

Use distinct representations such as `None = all` and `() = none`, or pass a separate `cancelled_activity_ids` or exclusion set.

References:

- `design.md:60-68`
- `design.md:124-130`
- `design.md:151-155`
- `specs/activity-manual-launch/spec.md:14-17`

## 3. High: The acknowledged cancel race can report CANCELLED while the activity runs

The scheduler changes the record to `CANCELLED` before the child acknowledges the signal. If the activity starts before `cancel_activity_signal` is processed, the handler only adds its ID to a set and does not cancel the current handle.

Its later `COMPLETED` update is then ignored because the scheduler considers `CANCELLED` terminal. The proposed mitigation of using `stop_activity` is no longer possible because the scheduler already reports the activity as cancelled.

Cancellation needs child acknowledgement or a transition such as `CANCEL_REQUESTED`, with the child deciding whether cancellation was accepted.

References:

- `design.md:124-130`
- `design.md:157-160`
- `tasks.md:61`
- `tasks.md:77-79`

## 4. High: The graceful-shutdown timeout does not measure activity cleanup

`ActivityHandle.cancel()` uses Temporal's default `TRY_CANCEL` semantics unless `ActivityCancellationType.WAIT_CANCELLATION_COMPLETED` is specified. Under `TRY_CANCEL`, awaiting the handle can raise cancellation immediately after the request, allowing the child to report `CANCELLED` before the activity worker has completed cleanup.

Consequently, the scheduler's 60-second wait normally succeeds immediately and cannot detect stuck cleanup. Cancelling the child workflow is also another cooperative cancellation request, not a guaranteed force cancellation.

The cancellation type, heartbeat requirements, cleanup acknowledgement, and fallback semantics must be specified explicitly.

References:

- `design.md:132-149`
- `specs/activity-stop/spec.md:34-60`
- `tasks.md:14`
- `tasks.md:32`
- `tasks.md:84`

## 5. High: A normal completion race can cancel the entire child workflow

After `stop_activity` validates `RUNNING`, the activity may complete before the stop signal reaches the child. The handler then does nothing because the activity is no longer current, while the scheduler waits only for `CANCELLED`.

A valid `COMPLETED` or `FAILED` transition never satisfies that predicate, causing a 60-second timeout followed by cancellation of the whole child, potentially while a later activity is running.

The wait condition must accept every terminal state and define what the Update returns when completion wins the race.

References:

- `design.md:142-149`
- `tasks.md:84-86`
- `tasks.md:110`

## 6. Medium: Failure leaves later activities permanently PENDING

The planned loop reports the current activity as `FAILED` and re-raises. All subsequent activities remain `PENDING` even though the child workflow has terminated and they can never execute.

The contract should define their terminal state, such as `SKIPPED` with an upstream-failure reason, and define the corresponding slot transition.

References:

- `tasks.md:32`
- `specs/activity-status-query/spec.md:34-37`
- `specs/activity-status-query/spec.md:72-75`

## Conclusion

The change should not proceed to implementation until the cancellation representation and cancel/stop race semantics are corrected.

No implementation or executable tests were present when this review was performed. The review covers the OpenSpec artifacts only.
