# python/xlnpwmon/__init__.py
"""
xlnpwmon - A simple power monitor for Xilinx.
"""
__version__ = "0.0.3" # Keep version consistent

try:
    # Import symbols from the compiled C++ module named '_core'
    from ._core import PowerMonitor, ErrorCode, SensorType, error_string

    # Define what gets imported with 'from xlnpwmon import *'
    __all__ = ['PowerMonitor', 'ErrorCode', 'SensorType', 'error_string', '__version__']

except ImportError as e:
    # If the core module cannot be imported, it's a critical failure.
    # Re-raise the ImportError or raise a custom exception.
    # Avoid setting PowerMonitor = None silently if the CLI depends on it.
    raise ImportError(
        f"Could not import the compiled core module (_core) for xlnpwmon: {e}. "
        "Please ensure the package was built and installed correctly."
    ) from e

# Optional: Add any pure Python helper functions or classes here if needed.