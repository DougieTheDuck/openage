# Tasks

- [x] 1.1 Define read-only local role/selection/kill/progress state bridge to QML (presenter publishes simulation-derived phase/percent and existing local identity, selection and score; actions remain explicit).
- [x] 1.2 Implement Builder Build Tower action and placement preview with invalid-site feedback (pending visual verification).
- [x] 1.3 Connect confirm/cancel UI actions to the existing simulation order path; preserve Ctrl-click for solo testing.
- [x] 1.4 Suppress world input when interacting with HUD and when canceling placement (hit-test the actual QML panel and capture button gestures; GUI verification remains under 1.6).
- [x] 1.5 Display the Builder's lifetime kill total and individual Tower kills; replicate owner-private Tower counts alongside each player's authoritative lifetime score.
- [ ] 1.6 Run the manual Qt Quick interaction checks in a graphical FVD build and execute build/tests/checkmerge (controller and HUD mouse capture regressions added; automated execution and manual GUI verification pending). See `verification.md`.

- [x] 1.7 Add local Shift-click FIFO queued construction, tile reservations, cancellation and regression tests. Network queue forwarding remains a separate integration.
- [x] 1.8 Render every queued site as a persistent pickable foundation; right-click to retask the owning Builder while canceling other jobs.
- [x] 1.9 Remove Ctrl-left-click construction and its Shift+Ctrl variant.
