# Spec Delta

## ADDED Requirements

### Requirement: Delete removes a foundation

A Builder player SHALL be able to remove one of their own foundations by
selecting it and pressing Delete. Its reserved tiles SHALL be freed and it
SHALL leave any construction queue. Other players' foundations SHALL be refused.

#### Scenario: Queued site
- **WHEN** a Builder deletes a foundation waiting in their queue
- **THEN** it disappears and the queue holds one site fewer

#### Scenario: Site being walked to
- **WHEN** a Builder deletes the foundation their Builder is walking to
- **THEN** it disappears and the Builder continues with the next queued site

#### Scenario: Box selection with buildings
- **WHEN** a player box-selects their unit together with buildings or foundations
- **THEN** only the unit is selected
