local summary = process_string("template={{ project_name }}\nslug={{ project_slug }}\nauthor={{ author }}\nci={{ include_ci }}\nprovider={{ ci_provider | default(\"\") }}\ndocs={{ docs_url | default(\"\") }}\n")
write_file(".tempify-summary.txt", summary)
