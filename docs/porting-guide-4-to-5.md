Title: GtkSourceView 4 -> 5 Migration Guide

# GtkSourceView 4 -> 5 Migration Guide

## GTK dependency

GtkSourceView 5 depends on GTK 4.

## GtkSourceGutterRenderer is a GtkWidget

In GtkSourceView 5, the [class@GutterRenderer] has become a widget.
This allows more flexibility by implementations to handle input as well
as cache snapshot information for faster rendering.

## GtkSourceGutterLines

Previously, GtkSourceView would spend a lot of time while scrolling just
for rendering the gutter. This was because it had to calculate much line
information on every frame (roughly 60 frames per second) and multiple
times within that frame.

In GtkSourceView 5, a [class@GutterLines] structure has been added to the
API which allows for collecting that information and re-using by each
gutter renderer, thus reducing overhead greatly.

[class@GutterRenderer] implementations should use this to store
information about each line and then recall it when snapshotting the
given line.

## GtkSourceSnippet

GtkSourceView 5 added support for snippets. This functionality did not
exist within previous versions of GtkSourceView.

The implementation contains a snippet language embedded within XML files
as well as a completion provider. You may also "push" snippets onto a
[class@View] manually by creating [class@SnippetChunk] and associating
them with a [class@Snippet]. Many completion providers may find this more
convenient to allow users to move through focus positions.

## GtkSourceCompletion

GtkSourceView 5 changed how completion providers are implemented. Doing so
has allowed for much reduction in overhead when generating results and
presenting them to the user. Additionally, it has improved Wayland support
for placing completion results.

[iface@CompletionProposal] no longer requires any implementation except
to implement the interface symbolically. That can be done using
`G_IMPLEMENT_INTERFACE(GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,NULL)`.
This allowed all of the implementation details to be placed in
[iface@CompletionProvider].

[class@CompletionCell] is a new display widget that is used
in multiple places as a generic container for information about completion
proposals.

[iface@CompletionProvider]s are now expected to asynchronously
provide a [iface@Gio.ListModel] of [iface@CompletionProposal]
instead of a `GList`. Refiltering of results can be provided using
the refilter method and should be updated while the user types further. If a
new model must be generated, use [method@CompletionContext.set_proposals_for_provider].

## Style Schemes

Since 5.2, GtkSourceView will synthesize an appropriate background color
for current-line-number if current-line-number’s background is not specified.
Style schemes which have not already set current-line-number should be updated
and use `#rgba(0,0,0,0)` to match the previous behavior.
