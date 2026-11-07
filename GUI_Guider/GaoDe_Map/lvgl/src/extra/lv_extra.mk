# Auto-collect all .c files under extra and its subdirectories (up to 3 levels).
# CSRCS must contain plain file names only (the simulator Makefile prefixes them
# with build/object/ and locates the sources via VPATH), so normalize the path
# separators and strip the directories. The VPATH entries are generated from
# the same file list, so new .c files (and new subdirectories) are picked up
# automatically without editing this file.
# Note: keep the patterns in one wildcard() call - this make version only
# resolves the multi-level "*" patterns when several patterns are given.
EXTRA_DIR := $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/extra
EXTRA_CSRCS := $(subst \,/,$(wildcard $(EXTRA_DIR)/*.c $(EXTRA_DIR)/*/*.c $(EXTRA_DIR)/*/*/*.c))
CSRCS += $(notdir $(EXTRA_CSRCS))
VPATH += :$(EXTRA_DIR)
VPATH += $(addprefix :,$(dir $(EXTRA_CSRCS)))
