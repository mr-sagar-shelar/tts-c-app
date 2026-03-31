# Typing API Assumptions

This document captures the assumptions used by the local typing learning module so a real backend can replace the current mock data later.

## Current placeholder behavior

- The app currently loads leaderboard data from the local file `typing_leaderboard_mock.json`.
- No network request is performed yet.
- The app compares the user's last completed typing session against the mock daily, weekly, and monthly leaderboards.
- A share action currently writes a plain text summary to `UserSpace/typing_share_latest.txt`.

## Expected API endpoints

### 1. Submit typing session

- Method: `POST`
- Path: `/api/v1/typing/sessions`
- Purpose: Save the result of a completed typing session.

Suggested request body:

```json
{
  "user_id": "device-or-user-id",
  "completed_at": "2026-03-31T10:15:00Z",
  "level_id": 4,
  "level_name": "Intermediate - Useful Words",
  "wpm": 28.4,
  "accuracy": 93.5,
  "elapsed_seconds": 91.2,
  "correct_chars": 216,
  "target_chars": 231,
  "typed_chars": 223,
  "prompt_replays": 2,
  "input_mode": "keyboard"
}
```

Suggested response body:

```json
{
  "ok": true,
  "session_id": "sess_123",
  "personal_best_speed": false,
  "personal_best_accuracy": true,
  "new_badges": [
    {
      "key": "accuracy_90",
      "title": "Steady Hands",
      "description": "Reached at least 90% accuracy."
    }
  ]
}
```

### 2. Leaderboard

- Method: `GET`
- Path: `/api/v1/typing/leaderboard`
- Query params:
  - `period`: `daily`, `weekly`, or `monthly`
  - `limit`: optional, default `5`

Suggested response body:

```json
{
  "period": "daily",
  "generated_at": "2026-03-31T10:30:00Z",
  "entries": [
    {
      "rank": 1,
      "name": "Asha",
      "wpm": 41.5,
      "accuracy": 98,
      "badge": "Trailblazer"
    }
  ],
  "user_comparison": {
    "last_wpm": 28.4,
    "last_accuracy": 93.5,
    "estimated_rank": 4
  }
}
```

### 3. Badge catalog

- Method: `GET`
- Path: `/api/v1/typing/badges`
- Purpose: Fetch badge titles, descriptions, and unlock thresholds.

Suggested response body:

```json
{
  "badges": [
    {
      "key": "first_finish",
      "title": "First Finish",
      "description": "Completed the first typing session."
    }
  ]
}
```

### 4. Share result

- Method: `POST`
- Path: `/api/v1/typing/share`
- Purpose: Create a share token, public card, or messaging payload for the latest session.

Suggested request body:

```json
{
  "user_id": "device-or-user-id",
  "session_id": "sess_123",
  "channel": "generic"
}
```

Suggested response body:

```json
{
  "ok": true,
  "share_text": "I just completed a typing session at 28.4 WPM with 93.5% accuracy.",
  "share_url": "https://example.org/share/abc123"
}
```

## User identity assumptions

- The current local build does not have authenticated users yet.
- A future implementation may use:
  - device-level user ids
  - school-managed ids
  - caregiver/teacher linked accounts

## Accessibility assumptions

- Every response should provide short text fields that can be read directly through speech output.
- Badge titles and descriptions should stay brief and encouraging.
- Leaderboard responses should avoid overly dense payloads because the UI is speech-first.

## Offline behavior assumptions

- If the server is unavailable, the app should continue saving sessions locally to `typing_progress.json`.
- Leaderboard and badge screens should fall back to cached or bundled data when live data is unavailable.
