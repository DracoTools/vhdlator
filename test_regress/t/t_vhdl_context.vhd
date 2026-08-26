-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A context declaration is a name for a set of context items (13.3), and a
-- context reference means those items written out where the reference stands
-- (13.4). It is not a use clause: the declaration has nothing of its own to
-- make visible, and what it names may be in another library. This is how UVVM
-- and OSVVM give a testbench everything it needs in one line.

library mylib;
context mylib.my_context;

package user_pkg is
  -- Names a type the context brought in, with no use clause of its own
  function pick return t_colour;
end package;

package body user_pkg is
  function pick return t_colour is
  begin
    return next_colour(DEFAULT_COLOUR);
  end function;
end package body;

library mylib;
context mylib.my_context;
use work.user_pkg.all;
use std.env.all;

entity t_vhdl_context is
end entity;

architecture tb of t_vhdl_context is
  signal c : t_colour := DEFAULT_COLOUR;
begin

  main : process is
    variable l : line;                 -- from std.textio, also via the context
  begin
    assert c = green report "default" severity failure;
    assert pick = blue report "pick" severity failure;
    assert next_colour(blue) = red report "wrap" severity failure;
    write(l, string'("context ok"));
    assert l'length = 10 report "textio through the context" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
