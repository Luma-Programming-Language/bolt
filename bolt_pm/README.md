# The bolt package manager

The goal of the bolt package manager is to download deps defined under the deps: tag in the build.bolt.

It enables commands like bolt get, bolt update, and bolt verify.

1. bolt get has a goal of downloading and setting up defined deps in the deps: section in build.bolt.
2. bolt update will first check if a new version of a dep is available and if so go ahead and update locally.
3. bolt verify will verify that a dep is stable, setup correctly, or has not been flagged as vulnerable.
