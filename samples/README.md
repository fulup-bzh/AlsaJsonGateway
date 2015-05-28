AJG json example of response to REST request.

Place those samples under sessiondir/fakemod to run gateway in --fakemod

Examples:
      DEBUG ../built/ajg-daemon --fakemod --verbose --port=1235 --rootdir=/opt/ajg-daemon/www --sessiondir=$HOME/.ajg/sessions
      PROD  /opt/ajg-daemon/bin/ajg-daemon --fakemod --port=1235 --rootdir=/opt/ajg-daemon/www --sessiondir=$HOME/.ajg/sessions --daemon --restart