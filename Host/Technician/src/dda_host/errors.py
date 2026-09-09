"""Exceptions shared by the supported DDA host API."""


class BoardError(RuntimeError):
    """Base error raised by board operations."""


class BoardProtocolError(BoardError):
    """The board returned malformed or inconsistent data."""


class BoardRejectedError(BoardError):
    """The board rejected a valid request."""


class BoardTimeoutError(BoardError):
    """The board did not complete an operation before its deadline."""
