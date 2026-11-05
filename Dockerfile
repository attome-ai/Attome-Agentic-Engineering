# J1: Attome Agentic Engineering on Linux or macOS, with the engine built inside the image.
#   docker build -t attome-ae .
#   docker run --rm attome-ae python scripts/demo.py preflight
#   docker run --rm -p 127.0.0.1:8765:8765 attome-ae python -m structura.app.rest --port 8765
FROM mambaorg/micromamba:2.0.5
USER root
ENV MAMBA_ROOT_PREFIX=/opt/mamba CONDA_PREFIX=/opt/mamba/envs/attome-ae
ENV PATH=/opt/mamba/envs/attome-ae/bin:$PATH
WORKDIR /attome
COPY environment.yml .
RUN micromamba create -y -n attome-ae -f environment.yml && \
    micromamba install -y -n attome-ae -c conda-forge cmake "cxx-compiler" && \
    micromamba clean -a -y
COPY engine engine
COPY families families
COPY data data
COPY docs docs
COPY structura structura
COPY scripts scripts
COPY tests tests
COPY pyproject.toml README.md ./
# through micromamba run for the conda compiler; the archiver has a prefixed name that cmake does not find by itself
RUN micromamba run -n attome-ae cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$CONDA_PREFIX \
      -DCMAKE_AR=$(ls $CONDA_PREFIX/bin/*-gcc-ar | head -1) -DCMAKE_RANLIB=$(ls $CONDA_PREFIX/bin/*-gcc-ranlib | head -1) && \
    micromamba run -n attome-ae cmake --build engine/build
# gmsh and CalculiX stay separate programs (GPL); see docs/licences.md
CMD ["python", "scripts/demo.py", "preflight"]
