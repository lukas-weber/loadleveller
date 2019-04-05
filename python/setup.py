import setuptools

setuptools.setup(
    name="loadleveller",
    version="2.0.0",
    author="Lukas Weber",
    author_email="lweber@physik.rwth-aachen.de",
    description="Python tools for the loadleveller library",
    url="https://git.rwth-aachen.de/lukas.weber2/load_leveller",
    packages=setuptools.find_packages(),
    license="MIT",
    scripts=["ydelete","yrun","ystatus"],
    install_requires=["pyyaml","h5py","numpy"]
)
