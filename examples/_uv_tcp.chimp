use io;
use _uv;

main argv {
  var port = 43456;

  #
  # for kicks, we'll serve our response messages from a background task.
  #
  var msg_svc = spawn {
    while true {
      match recv() {
        t { t.send("Hello World"); }
      }
    }
  };

  #
  # get the default event loop & bind a TCP socket.
  #
  var loop = _uv.default_loop();
  var srv = loop.tcp();
  io.print(str("starting server on port ", port));
  srv.bind(port).listen(fn { |c|
    #
    # client accepted! let the background task know we want a message.
    #
    msg_svc.send(self());
    var msg = recv();

    #
    # send the message & close the socket.
    #
    c.write(msg);
    c.on_read(fn { |chunk|
      io.write(chunk);
    });
    c.on_end(fn {
      c.close();
      io.print("client disconnected");
    });
  });

  #
  # testing for GC bugs: the loop should have
  # a ref to the server object.
  #
  srv = nil;

  #
  # serve forever.
  #
  loop.run();
}

