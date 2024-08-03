[![clang-format Check](https://github.com/tizilogic/krass/actions/workflows/clang-format.yml/badge.svg)](https://github.com/tizilogic/krass/actions/workflows/clang-format.yml) [![Linux (OpenGL)](https://github.com/tizilogic/krass/actions/workflows/linux-opengl.yml/badge.svg)](https://github.com/tizilogic/krass/actions/workflows/linux-opengl.yml) [![Windows (Direct3D 11)](https://github.com/tizilogic/krass/actions/workflows/windows-direct3d11.yml/badge.svg)](https://github.com/tizilogic/krass/actions/workflows/windows-direct3d11.yml)

# krass - Single texture runtime asset packer for krink

Library to pack assets such as fonts and images into a single texture at runtime with the goal to
avoid pipeline switching and optimize batched rendering in
[krink](https://github.com/tizilogic/krink) apps.

## To use krass in a krink project

Add this repo as submodule to your project (or simply copy the contents) and add this to your
`kfile.js`:

```js
// needed to not add "krink" as subproject when evaluating the kfile from krass
global.noKrinkPlease = true;

// Assumes your project var is called "project"
let krass = await project.addProject('path/to/krass');
krass.useAsLibrary();
```
