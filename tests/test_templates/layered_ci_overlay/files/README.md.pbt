# {{ project_name }}

- Standard: {{ language_standard }}
- Layer: ci
- CI: {{ if include_ci }}{{ ci_provider }}{{ else }}disabled{{ endif }}
