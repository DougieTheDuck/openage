# Design notes

## Network

- New command `CHAT = 14` (guest → host) carrying UTF-8 text. The host:
  - rejects the Hunter and anyone not in the match;
  - strips control characters and trims whitespace; rejects empty text;
  - rejects text over 140 **characters** (Unicode code points, not bytes);
  - limits each player to 5 messages per 10 seconds.
- `WorldState` gains `std::vector<ChatMessage> chat`:
  `{message_id, sender_id, text, sent_at}`, the last 20 messages. Monotonic
  `message_id` so each is shown once.
- Recipient filtering as for flares: Builders receive team chat; the
  Hunter's world never contains it (snapshots and deltas, and the host's
  local view when the host plays the Hunter).
- Protocol: bump `PROTOCOL_MAJOR`. Combine with `builder-flare` if both land
  together so players only rebuild once.
- Sender names: `Player N` for now. Lobby display names could replace this
  later.

## Client

- Enter opens a QML `TextInput` (maxLength 140) at the chat position. Focus
  rules follow the lobby's text fields: while it has focus the presenter does
  not pass keys to gameplay bindings (the same guard the lobby uses), so
  typing "b" or "g" does nothing in the world. Enter sends and closes; Esc
  closes without sending; an empty line just closes.
- Message list: QML `Column` anchored left (margin ~16 px), top at
  `height * 0.75`, growing downward; each line bold, `flagColor(sender)`,
  dark outline for readability over terrain; fades after 10 s.
- The Hunter: Enter does nothing; no chat UI.
- Solo play: messages loop back locally (useful for testing).
- Optional: a soft `chat` sound cue on receive.

## Open questions

- Should Builders also be able to send to all players (including the
  Hunter), e.g. Shift+Enter, like AoE2's "all" chat?
- Should the Hunter have any chat at all (e.g. taunts to everyone)?
