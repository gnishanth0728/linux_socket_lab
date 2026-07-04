from __future__ import annotations

import html
from pathlib import Path
from typing import Iterable

from timeline import Event


def write_html_report(events: Iterable[Event], output_path: Path | str) -> None:
    output = Path(output_path)
    rows = []
    for event in events:
        rows.append(
            f"<tr class=\"{html.escape(event.layer)}\"><td>{html.escape(event.layer)}</td><td>{html.escape(event.name)}</td><td>{event.timestamp:.3f}</td><td>{html.escape(event.details)}</td></tr>"
        )

    html_text = f"""<!doctype html>
<html lang=\"en\">
<head>
  <meta charset=\"utf-8\">
  <title>HTTP Observer Timeline</title>
  <style>
    body {{ font-family: Arial, sans-serif; margin: 2rem; }}
    select {{ margin-bottom: 1rem; padding: 0.4rem; }}
    table {{ border-collapse: collapse; width: 100%; }}
    th, td {{ border: 1px solid #ccc; padding: 0.5rem; text-align: left; }}
    th {{ background: #f2f2f2; }}
  </style>
</head>
<body>
  <h1>HTTP Observer Timeline</h1>
  <label for=\"layerFilter\">Filter by layer:</label>
  <select id=\"layerFilter\" onchange=\"filterRows(this.value)\">
    <option value=\"all\">All layers</option>
    <option value=\"application\">Application</option>
    <option value=\"http\">HTTP</option>
    <option value=\"syscall\">Syscall</option>
    <option value=\"tcp\">TCP</option>
    <option value=\"socket\">Socket</option>
    <option value=\"kernel\">Kernel</option>
  </select>
  <table>
    <thead><tr><th>Layer</th><th>Name</th><th>Timestamp</th><th>Details</th></tr></thead>
    <tbody>
      {''.join(rows)}
    </tbody>
  </table>
  <script>
    function filterRows(layer) {{
      const rows = document.querySelectorAll('tbody tr');
      rows.forEach(row => {{
        if (layer === 'all' || row.className === layer) {{
          row.style.display = '';
        }} else {{
          row.style.display = 'none';
        }}
      }});
    }}
  </script>
</body>
</html>
"""
    output.write_text(html_text, encoding="utf-8")
