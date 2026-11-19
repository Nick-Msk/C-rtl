import workspace
workspace.add_projects_root(".")     # файл лежит в ngutils → корень = ngutils
workspace.add_projects("bool", "checker", "common", "error", "fs2", "log", "path")

