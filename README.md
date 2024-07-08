# krass - Single texture runtime asset packer for krink

Library to pack assets such as fonts and images into a single texture at runtime with the goal to
avoid pipeline switching and optimize batched rendering in
[krink](https://github.com/tizilogic/krink) apps.

## To use krass in a krink project

Add this repo as submodule to your project (or simply copy the contents) and add this to your
`kfile.js`:

```js
global.noKrinkPlease = true; // needed to not add "krink" as subproject when evaluating the kfile from krass
let krass = await project.addProject('path/to/krass');  // Assumes your project var is called "project"
krass.useAsLibrary();
```
