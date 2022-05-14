# CI containers

In order to build the image used by the CI run the following:

```shell
./run-docker.sh build --base=fedora
```

And to publish the image

```shell
./run-docker.sh push --base=fedora
```

For more details, you can run `./run-docker.sh help`
