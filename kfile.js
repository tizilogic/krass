let project = new Project('krass');

project.useAsLibrary = () => {
	project.debugDir = null;
    project.addExclude('tests/basic.c');
};

project.addFile('src/krass.c');
project.addFile('tests/basic.c');
project.addIncludeDir('src');
project.setDebugDir('tests/bin');

if (typeof noKrinkPlease === 'undefined') {
    await project.addProject('krink');
}

project.addDefine("KR_FULL_RGBA_FONTS");

project.setCStd('c99');
project.setCppStd('c++11');
project.flatten();

resolve(project);
