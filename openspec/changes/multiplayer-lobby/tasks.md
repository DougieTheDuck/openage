# Tasks

- [ ] 1.1 Provide a Windows direct-IP/LAN host/listen and join/connect entry point (a startup Qt Quick Host/Join menu for pre-provisioned *relay* pairs is present; standalone LAN hosting remains incomplete).
- [ ] 1.2 Show participants, reject overflow/incompatible clients and handle pre-start departures (2-person provisioned relay UI can show participants; 8-person remote orchestration and UI are pending).
- [ ] 1.3 Add a host-only Start action with a minimum of two humans; do not require ready flags (the Start Game UI triggers the single authoritative role draw and five-second reveal on both relay peers; synchronizing bootstrap works, but playable live gameplay remains pending).
- [x] 1.4 Implement and test the 0.5:1 weighted Hunter draw using deterministic injected randomness (host-only pre-match core; UI wiring remains outstanding).
- [ ] 1.5 Assign one human to slot 1, others to unique slots 2-8, and broadcast the same result (assignment, validation and StartMatch codec implemented; socket broadcast/integration still pending).
- [ ] 1.6 Initialize only participating players and their starting Builders; remove online role switching.
- [ ] 1.7 Preserve the separate solo no-AI hotkey mode (players 1/2).
- [ ] 1.8 Validate a two-computer Windows host/join/start flow, then test maximum eight humans.
