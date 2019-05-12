#include <signal.h>

#include "dispatcher.h"
#include "cache.h"
#include "Resolver.h"
#include "sig.h"

void sig_handler(int sig)
{
  dbg("got signal = %d", sig);

  if (SIGHUP == sig)
  {
    // Reload driver configurations
    resolver::instance()->configure();
    return;
  }

  lnp_cache::instance()->stop();
  dispatcher::instance()->stop();
}

void set_sighandlers()
{
  static int sigs[] = {SIGTERM, SIGINT, SIGHUP, 0 };

  for (int * sig = sigs; *sig; sig++)
  {
    if (SIG_IGN == signal(*sig, sig_handler))
    {
      signal(*sig,SIG_IGN);
    }
  }
}
