# ai-checker/Makefile — AI review / fix / status pipeline.
#

AI_CHECKER_DIR := $(CURDIR)
AI_CHECKER_ROOT ?= $(abspath ..)
include ai-checker.mk

AI_PROJECTS ?=

.PHONY: help _targets
.DEFAULT_GOAL := help
help: ai-help
_targets: _ai_targets
