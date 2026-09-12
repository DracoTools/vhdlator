-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A conversion written on the formal part of a port association converts the
-- value on its way out; one written on the actual converts it on its way in,
-- and an association with a conversion is still an association with the
-- signal inside it (6.5.7.1) -- so it is that signal the port drives.
--
-- A port of mode inout may carry a conversion each way at once. What it
-- drives still reaches the actual through the formal's conversion; the other
-- direction is a driver of the port, to be resolved with the entity's own
-- (6.5.6.3), and this model does not carry it.

package conv_pkg is
  type t_rec is record
    n : integer;
  end record;
  function to_rec (n : integer) return t_rec;
  function to_int (r : t_rec) return integer;
end package;

package body conv_pkg is
  function to_rec (n : integer) return t_rec is
  begin
    return (n => n);
  end function;
  function to_int (r : t_rec) return integer is
  begin
    return r.n;
  end function;
end package body;

use work.conv_pkg.all;

entity t_vhdl_portconv_sub is
  port ( f1 : in integer;
         f2 : out integer;
         f3 : out integer );
end entity;

architecture rtl of t_vhdl_portconv_sub is
begin
  f2 <= f1 + 1;
  f3 <= f1 + 2;
end architecture;

use work.conv_pkg.all;
use std.env.all;

entity t_vhdl_portconv is
end entity;

architecture tb of t_vhdl_portconv is
  signal s1 : t_rec := (n => 5);
  signal s2 : t_rec;
  signal s3 : t_rec;
begin

  u : entity work.t_vhdl_portconv_sub
    port map (
      -- In: the conversion on the actual converts on the way in
      f1 => to_int(s1),
      -- Out with a conversion on the formal, driven from f1
      to_rec(f2) => s2,
      -- Out: the conversion on the formal converts on the way out
      to_rec(f3) => s3);

  process is
  begin
    wait for 1 ns;
    assert s2.n = 6 report "s2 is " & integer'image(s2.n) severity failure;
    assert s3.n = 7 report "s3 is " & integer'image(s3.n) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
