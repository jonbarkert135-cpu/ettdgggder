# 029 — Customization without restart

**Roadmap item 29.** Status: enforced by the type system and a test.

`ApplyKind` has exactly two values:

```cpp
enum class ApplyKind { kRepaint, kRelayout };
```

There is no `kRestartRequired`. A property that cannot be applied live does not belong in the
theme system, and the test walks every property to assert it is classified — so adding one forces
the author to say which of the two it is, rather than quietly shipping a "restart to apply" label.

- **Repaint**: colours, transparency, blur, animation level. New values are pushed into the
  compositor; no layout is invalidated.
- **Relayout**: density, icon size, corner radius, font scale, spacing, compact and immersive
  mode, sidebar visibility, tab shape. The widget tree is measured again; nothing is recreated.

Tab layout (horizontal ↔ vertical) is the same story one level up: both layouts render the *same*
`TabModel`, so switching is a relayout rather than a rebuild, and no tab state is lost.

The honest exceptions, and why they are not in this enum: changing the *profile* and toggling
hardware acceleration both restart the renderer. Neither is a theme property; both say so where
they live.
