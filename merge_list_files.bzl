def merge_list_files(name, files):
    native.genrule(
        name = name,
        srcs = files,
        outs = ["merged_{}".format(name)],
        cmd_bash = """
          for list in $(SRCS); do
            cat "$$list" >> "$@"
          done
        """
    )
