# E-ink dashboard app

This is an app for the [LilyGo T5 4.7" e-ink board](https://t5-47-t5-47-plus.readthedocs.io/en/latest/) which downloads a PNG file from the server and displays it on the screen.

The server is responsible for generating a PNG file with the information you'd like to see in the dashboard. This makes the application running on the e-ink board fairly simple.

## Building

1. Create a `.env` file — see [.env.sample](.env.sample) for a template.
2. `idf.py build flash monitor` as usual.


