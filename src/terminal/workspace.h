/* Bounded presentation topology only. Contains no PTY, process or model owner. */
#ifndef AETHERI_TERMINAL_WORKSPACE_H
#define AETHERI_TERMINAL_WORKSPACE_H
#include <stdbool.h>
#include <stdint.h>
enum { AE_TERM_PANES=8,AE_TERM_NODES=15,AE_TERM_TABS=8 };
enum ae_terminal_partition { AE_PART_FREE,AE_PART_LEAF,AE_PART_COLUMNS,AE_PART_ROWS };
struct ae_terminal_partition_node {enum ae_terminal_partition kind;unsigned first,second,slot;uint64_t id;float ratio;};
struct ae_terminal_tab {unsigned root,focus;uint64_t id;};
struct ae_terminal_workspace {
    struct ae_terminal_partition_node nodes[AE_TERM_NODES];
    struct ae_terminal_tab tabs[AE_TERM_TABS];unsigned count,active;uint64_t serial;
};
struct ae_terminal_area {float x,y,w,h;};
struct ae_terminal_workspace_layout {
    struct ae_terminal_area panes[AE_TERM_PANES],bounds[AE_TERM_NODES],dividers[AE_TERM_NODES];unsigned visible;
};
/* Initialize a NEW value; never reset a live workspace's identity counter. */
void ae_terminal_workspace_init(struct ae_terminal_workspace *);
bool ae_terminal_workspace_valid(const struct ae_terminal_workspace *);
/* Atomic value mutations; failure leaves the topology and ID counter unchanged.
   slot is the host's stable session slot; the host retains all lifecycle duties.
   A removed slot can be reused only with a new, nonreused node identity. */
bool ae_terminal_workspace_tab(struct ae_terminal_workspace *,unsigned slot);
bool ae_terminal_workspace_split(struct ae_terminal_workspace *,unsigned slot,bool columns);
bool ae_terminal_workspace_remove(struct ae_terminal_workspace *,unsigned slot);
bool ae_terminal_workspace_select(struct ae_terminal_workspace *,unsigned tab);
bool ae_terminal_workspace_focus(struct ae_terminal_workspace *,unsigned slot);
unsigned ae_terminal_workspace_focused(const struct ae_terminal_workspace *);
unsigned ae_terminal_workspace_order(const struct ae_terminal_workspace *,unsigned slots[AE_TERM_PANES]);
/* Layout clamps a remembered ratio to BOTH children's current minimum sizes.
   An undersized viewport fails atomically; the host owes a readable fallback.
   Only active-tab panes are projected. No hidden PTY is resized or drained. */
bool ae_terminal_workspace_layout(const struct ae_terminal_workspace *,struct ae_terminal_area,
    float min_width,float min_height,float gutter,struct ae_terminal_workspace_layout *);
bool ae_terminal_workspace_ratio(struct ae_terminal_workspace *,unsigned node,float ratio);
#endif
