-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A package's table of records of numbers, and a record constant, read by
-- functions that size signals of an instance from a name its generic gives
-- (14.4.2): each instance's width is the one its name looks up. open-logic's
-- fixed-point approximations look their formats up by table name, and the
-- CIC filters read the package's "FixFmt_Unused_c".

package t_vhdl_pkgtable_pkg is
  type fmt_t is record
    s : natural;
    i : integer;
    f : integer;
  end record;
  type fmts_t is array (natural range <>) of fmt_t;
  constant Fmts_c : fmts_t(0 to 2) := ((0, 1, 13), (0, 1, 17), (1, 2, 21));
  constant Unused_c : fmt_t := (0, 1, 0);
  function tableIndex(name : string) return natural;
  function width_of(name : string) return natural;
  function unused_width return natural;
end;
package body t_vhdl_pkgtable_pkg is
  function tableIndex(name : string) return natural is
  begin
    if name = "a" then return 0;
    elsif name = "bb" then return 1;
    end if;
    return 2;
  end function;
  function width_of(name : string) return natural is
    constant f : fmt_t := Fmts_c(tableIndex(name));
  begin
    return f.s + f.i + f.f;
  end function;
  function unused_width return natural is
  begin
    return Unused_c.s + Unused_c.i + Unused_c.f;
  end function;
end;
use work.t_vhdl_pkgtable_pkg.all;
entity t_vhdl_pkgtable_m is
  generic (Name_g : string := "a");
  port (w : out natural; u : out natural);
end;
architecture a of t_vhdl_pkgtable_m is
  constant W_c : natural := width_of(Name_g);
  signal v : bit_vector(W_c - 1 downto 0);
  signal x : bit_vector(unused_width downto 0);
begin
  w <= v'length;
  u <= x'length;
end;
entity t_vhdl_pkgtable is
end entity;
architecture a of t_vhdl_pkgtable is
  signal w1, w2, w3, u1 : natural;
begin
  u_1 : entity work.t_vhdl_pkgtable_m generic map (Name_g => "a") port map (w1, u1);
  u_2 : entity work.t_vhdl_pkgtable_m generic map (Name_g => "bb") port map (w2, open);
  u_3 : entity work.t_vhdl_pkgtable_m generic map (Name_g => "zzz") port map (w3, open);
  process
  begin
    wait for 1 ns;
    assert w1 = 14 and w2 = 18 and w3 = 24 and u1 = 2
      report "w=" & integer'image(w1) & " " & integer'image(w2) & " " & integer'image(w3)
      & " u=" & integer'image(u1) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end;
