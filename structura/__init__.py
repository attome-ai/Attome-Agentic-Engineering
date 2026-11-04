"""Attome Agentic Engineering. `from structura import Study` is the Python door (I2); it imports lazily so the
engine wrapper stays light for scripts that do not need the agent layer."""


def __getattr__(name):
    if name == "Study":
        from structura.sdk import Study
        return Study
    if name == "sdk":
        import importlib
        return importlib.import_module("structura.sdk")
    raise AttributeError(name)
