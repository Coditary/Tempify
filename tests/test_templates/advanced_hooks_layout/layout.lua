return {
  {
    source = "docs/readme.txt.pbt",
    target = "README.md.pbt",
    render = true,
  },
  {
    source = "raw/static.txt.pbt",
    target = "static/output.txt",
    render = false,
  },
  {
    source = "notes/todo.txt.pbt",
    exclude = true,
  },
}
