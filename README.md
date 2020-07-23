# G3D Development Repository

This is my (Josef's) development repository for G3D development. It is a greatly reduced set of files from the subversion repository in an effort to make branched development lighter weight. The core repository remains the [sourceforge repository](https://sourceforge.net/projects/g3d/).

To use this repository, clone it in whatever location you want, then take just the `.git` directory and copy it into the location where you have G3D checkout out from subversion. You will then be able to `svn up` to get the latest from subversion `svn commit` to commit the latest changes to subversion as well as use all of the git magic you want for branched development. There may still be some sloppiness when switching between committing/pushing to svn/git, particularly when new stuff is committed to svn.

