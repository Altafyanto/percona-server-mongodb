/* libunwind - a platform-independent unwind library
   Copyright (c) 2002-2004 Hewlett-Packard Development Company, L.P.
        Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

   Modified for x86_64 by Max Asbock <masbock@us.ibm.com>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <stdlib.h>

#include "unwind_i.h"

#ifndef UNW_REMOTE_ONLY

HIDDEN inline int
s390x_local_resume (unw_addr_space_t as, unw_cursor_t *cursor, void *arg)
{
  struct cursor *c = (struct cursor *) cursor;
  ucontext_t uc = *c->uc;
  ucontext_t *rt = NULL;
  struct sigcontext *sc = NULL;
  int i;
  unw_word_t sp, ip;
  uc.uc_mcontext.psw.addr = c->dwarf.ip;

  /* Ensure c->pi is up-to-date.  On x86-64, it's relatively common to
     be missing DWARF unwind info.  We don't want to fail in that
     case, because the frame-chain still would let us do a backtrace
     at least.  */
  dwarf_make_proc_info (&c->dwarf);

  switch (c->sigcontext_format)
    {
    case S390X_SCF_NONE:
      Debug (8, "resuming at ip=%llx via setcontext()\n",
                (unsigned long long) c->dwarf.ip);
      setcontext (&uc);
      abort(); /* unreachable */
    case S390X_SCF_LINUX_SIGFRAME:
      Debug (8, "resuming at ip=%llx via signal trampoline\n",
                (unsigned long long) c->dwarf.ip);
      sc = (struct sigcontext*)c->sigcontext_addr;
      for (i = UNW_S390X_R0; i <= UNW_S390X_R15; ++i)
        sc->sregs->regs.gprs[i-UNW_S390X_R0] = uc.uc_mcontext.gregs[i-UNW_S390X_R0];
      for (i = UNW_S390X_F0; i <= UNW_S390X_F15; ++i)
        sc->sregs->fpregs.fprs[i-UNW_S390X_F0] = uc.uc_mcontext.fpregs.fprs[i-UNW_S390X_F0].d;
      sc->sregs->regs.psw.addr = uc.uc_mcontext.psw.addr;

      sp = c->sigcontext_sp;
      ip = c->sigcontext_pc;
      __asm__ __volatile__ (
        "lgr 15, %[sp]\n"
        "br %[ip]\n"
        : : [sp] "r" (sp), [ip] "r" (ip)
      );
      abort(); /* unreachable */
    case S390X_SCF_LINUX_RT_SIGFRAME:
      Debug (8, "resuming at ip=%llx via signal trampoline\n",
                (unsigned long long) c->dwarf.ip);
      rt = (ucontext_t*)c->sigcontext_addr;
      for (i = UNW_S390X_R0; i <= UNW_S390X_R15; ++i)
        rt->uc_mcontext.gregs[i-UNW_S390X_R0] = uc.uc_mcontext.gregs[i-UNW_S390X_R0];
      for (i = UNW_S390X_F0; i <= UNW_S390X_F15; ++i)
        rt->uc_mcontext.fpregs.fprs[i-UNW_S390X_F0] = uc.uc_mcontext.fpregs.fprs[i-UNW_S390X_F0];
      rt->uc_mcontext.psw.addr = uc.uc_mcontext.psw.addr;

      sp = c->sigcontext_sp;
      ip = c->sigcontext_pc;
      __asm__ __volatile__ (
        "lgr 15, %[sp]\n"
        "br %[ip]\n"
        : : [sp] "r" (sp), [ip] "r" (ip)
      );
      abort(); /* unreachable */
    }
  return -UNW_EINVAL;
}

#endif /* !UNW_REMOTE_ONLY */

/* This routine is responsible for copying the register values in
   cursor C and establishing them as the current machine state. */

static inline int
establish_machine_state (struct cursor *c)
{
  int (*access_reg) (unw_addr_space_t, unw_regnum_t, unw_word_t *,
                     int write, void *);
  int (*access_fpreg) (unw_addr_space_t, unw_regnum_t, unw_fpreg_t *,
                       int write, void *);
  unw_addr_space_t as = c->dwarf.as;
  void *arg = c->dwarf.as_arg;
  unw_fpreg_t fpval;
  unw_word_t val;
  int reg;

  access_reg = as->acc.access_reg;
  access_fpreg = as->acc.access_fpreg;

  Debug (8, "copying out cursor state\n");

  for (reg = 0; reg <= UNW_REG_LAST; ++reg)
    {
      Debug (16, "copying %s %d\n", unw_regname (reg), reg);
      if (unw_is_fpreg (reg))
        {
          if (tdep_access_fpreg (c, reg, &fpval, 0) >= 0)
            (*access_fpreg) (as, reg, &fpval, 1, arg);
        }
      else
        {
          if (tdep_access_reg (c, reg, &val, 0) >= 0)
            (*access_reg) (as, reg, &val, 1, arg);
        }
    }

  if (c->dwarf.args_size)
    {
      if (tdep_access_reg (c, UNW_S390X_R15, &val, 0) >= 0)
        {
          val += c->dwarf.args_size;
          (*access_reg) (as, UNW_S390X_R15, &val, 1, arg);
        }
    }
  return 0;
}

int
unw_resume (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  int ret;

  Debug (1, "(cursor=%p)\n", c);

  if ((ret = establish_machine_state (c)) < 0)
    return ret;

  return (*c->dwarf.as->acc.resume) (c->dwarf.as, (unw_cursor_t *) c,
                                     c->dwarf.as_arg);
}
