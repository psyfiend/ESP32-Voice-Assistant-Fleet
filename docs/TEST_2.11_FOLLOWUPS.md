# TEST - the 2.11 follow-ups (your notes on v0.2.19)

Branch `feat/2.11-followups`, off `main` at `v0.2.19`. Round 1 (below) built your first notes; round
2 is your redesign of moving a card, in your words (the demo artifact "Card Moves on the Glass" and
your answers of 2026-10-31). The 4880's portrait look and the rotation switcher wait.

## Round 2 - what changed

| Where | What it does now |
|---|---|
| **A tile's Visibility** | **Hide from the cluster** (or Show in the cluster), **Add to dashboard**, **Change cluster**, one column as before. **Restore**, small and red, on the bottom row's right, only on a card with something to restore. The helper note as it was. |
| **A page card's Visibility** | **Hide from this page**, **Change group**, **Add to cluster**, and Restore the same way. A cluster's own Visibility keeps Hide from this page alone. |
| **A group card's member** (Desk's lamp) | Add card to dashboard opens the Add to dashboard view, Copy only, with "Members of a group card cannot be moved, only copied." |
| **Add to dashboard** | "Select group for this card:", this page's groups (the card's own marked `*`), **New group**, **None**; your New group / None line; Copy (default) / Move beside a blue **Next**. If the entity is on the dashboard already, only your "You cannot add this card..." line. |
| **Change cluster / Add to cluster** | "Select target cluster:", this page's clusters that can take it - hidden or empty ones marked "(h)" - never one that has it already; your permanent line; your italic line when there is none. Copy / Move and Next. |
| **Change group** | "Select group for this card:", the same grid and line, Next. Its dialog's button is **Confirm**. |
| **New group** | Next opens the keyboard, "New group name". The group is made when you confirm the dialog, in a colour no other group is using. |
| **The dialogs** | Your titles and lines exactly, the names in the accent colour, "A card can always be restored...", and for the last tile your "The cluster ... will be empty..." line. Buttons Cancel / Copy, Move or Confirm. |
| **Restore** | "Restore this card to its original configuration?" - for a copy, your Delete version (red Delete); for a moved card, your Restore version (red Restore), with its name line when its name changed, the hidden-cluster line or the renamed-cluster line; for a card whose group only changed, the group version. |
| **Moving back by hand** | Moving a card back into the cluster it came from (or a page card back to the page) **is** its Restore. |
| **An emptied cluster** | Moving its last tile out hides it. Adding a card to it, or restoring one into it, brings it back. |
| **Members** | A member in the header has the **header glyph, square**, outside its row on the right; the rows as before. **Both lists show HA's full name.** |
| **Gone** | Move this card, Move back to the cluster / page, Remove from the cluster / page: Restore does what they did. |

### How it is kept

- **Every card made here names its original** (a dashboard card). A made card that moves again
  **relocates** - page to cluster, cluster to page, cluster to cluster - so there is never a copy of
  a copy, and Restore is always one step: a copy is deleted; a moved card goes home and its original
  comes back.
- A dashboard member moved out remembers its cluster's name and group as they were (`home_name`,
  `home_group`). At Restore a different name means **renamed or repurposed**: the card goes onto the
  dashboard in its original group, with its original name. A cluster's id never changes on a rename,
  so the name is the only sign; one repurposed without a rename cannot be told.
- A page card moved into a cluster now records the card it became (`moved_to`), as members do - a
  copy of it can no longer be mistaken for the moved one.
- A dashboard card given another group keeps the one it had (`orig_group`); choosing that group again
  leaves nothing to restore.
- The development reset clears `home_name` and `home_group` with the rest.

## Round 2 - try it

On the P4_5 (or the 4B, or the 1060):

1. A tile > Visibility: three buttons, no Restore. **Change cluster** > a cluster > **Move** > Next:
   the dialog's words; Move. In its new cluster, Visibility shows **Restore** > the Restore version >
   Restore: back home.
2. Again with **Copy**: Restore on the copy shows the Delete version; Delete removes only the copy.
3. **Add to dashboard** > a group > Move > Next > Move: on the page; Restore puts it back in its
   cluster. With the same group, then another, then None: line 2 changes.
4. **New group** > Next > type a name > Save: the dialog names it; Copy makes the group and the card.
5. A page card > **Change group** > another group > Next > Confirm. Its Restore shows the group
   version and puts its group back.
6. A page card > **Add to cluster** > Move: off the page, in the cluster; its Restore puts it back.
7. Move a cluster's last tile out: the dialog's last line; the cluster disappears. It shows with
   "(h)" in Change cluster; adding a card brings it back.
8. Move a tile to another cluster, rename its first cluster, then Restore it: the renamed version;
   it lands on the dashboard.
9. Hide a cluster a card came from, then Restore the card: the hidden version; the cluster is back.
10. Desk's lamp > Add card to dashboard: Copy only, the members line.
11. Members of a cluster with header sensors: the glyph beside those rows; names in full in both lists.

## Round 2 - to confirm

Each is the nearest thing to your words that the case needed:

- **A group card's member, Copy line 1**: "This card will be copied from the **group card** Desk and
  placed onto the dashboard as a separate card." Your line says "the cluster"; Desk is a group card.
- **The italic line** shows in the dim tag face: the panels have no italic face, and one costs
  flash. Add one?
- **Copy only** for a group card's member is one Copy chip, not a greyed Move.
- **Restore when the original had no group**: "...back to its original group None."
- **No toasts** on these moves: the window closes and the page shows the result.

---

## Round 1 (built 2026-10-31, superseded by round 2)

| # | Your note | What it did |
|---|---|---|
| 1 | Moving a card from one cluster to another took two trips | "Move this card", with Change group? and Add to cluster? - replaced in round 2 |
| 2 | Which members are in a header slot | a "HEADER" tag - replaced by the glyph |
| 3 | Header options and Members named members differently | a typed name, else HA's - now HA's always |
| 4 | A swipe on a sheet with nothing to scroll counted as a tap | **In any open sheet, a finger that moves more than 4 mm is never a tap** - approved |

## Later, from the same notes

- The 4880 in portrait: font sizes and fit, far down the list.
- A rotation switcher: rotate-and-restart from UI Tweaks is small (the board already works out its
  frame path, layout and touch mapping from the rotation at boot); switching live, without a
  restart, is a larger job. With #71.
