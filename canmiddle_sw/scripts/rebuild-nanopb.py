# see https://github.com/mhaberler/nanopb-test/tree/master/scripts
Import("env")

env.AddCustomTarget(
    name="nanopb",
    dependencies=None,
    actions=[
       " (cd proto; "
       "python ../.pio/libdeps/esp32doit-devkit-v1/Nanopb/generator/nanopb_generator.py  "
       "--strip-path --output-dir=../lib/util message.proto)"
    ],
    title="Nanopb generate step",
    description="Rebuild .c/.h files from .proto"
    #always_build=True
)