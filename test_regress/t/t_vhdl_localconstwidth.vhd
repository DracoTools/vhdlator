-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice target whose width is a constant of the function, worked out from
-- a parameter that stands for the instance's generic: "res(pb_c - 1 downto
-- 0) := syn" with "pb_c := pbits(dw)". The width is the instance's, a part
-- select's, though the constant is a variable of the emitted function.
-- Shaped after open-logic's olo_ft_pkg_ecc eccSyndromeAndParity.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_localconstwidth_pkg is
  function pbits(w : positive) return positive;
  function synpar(cw : std_logic_vector; dw : positive) return std_logic_vector;
end package;
package body t_vhdl_localconstwidth_pkg is
  function pbits(w : positive) return positive is
    variable p : positive := 1;
  begin
    while 2 ** p < w + p + 1 loop
      p := p + 1;
    end loop;
    return p;
  end function;
  function synpar(cw : std_logic_vector; dw : positive) return std_logic_vector is
    constant pb_c : positive := pbits(dw);
    constant hl_c : positive := dw + pb_c;
    variable cwn : std_logic_vector(hl_c downto 0);
    variable syn : std_logic_vector(pb_c - 1 downto 0) := (others => '0');
    variable op : std_logic := '0';
    variable res : std_logic_vector(pb_c downto 0);
  begin
    cwn := cw;
    for p in 0 to pb_c - 1 loop
      for i in 1 to hl_c loop
        if (i / (2 ** p)) mod 2 = 1 then
          syn(p) := syn(p) xor cwn(i);
        end if;
      end loop;
    end loop;
    for i in 0 to hl_c loop
      op := op xor cwn(i);
    end loop;
    res(pb_c - 1 downto 0) := syn;
    res(pb_c) := op;
    return res;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_localconstwidth_pkg.all;
entity t_vhdl_localconstwidth_m is
  generic (Width_g : positive := 8);
  port (cw : in std_logic_vector(Width_g + pbits(Width_g) downto 0);
        sp : out std_logic_vector(pbits(Width_g) downto 0));
end entity;
architecture a of t_vhdl_localconstwidth_m is
begin
  sp <= synpar(cw, Width_g);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_localconstwidth is
end entity;
architecture a of t_vhdl_localconstwidth is
  signal sp : std_logic_vector(4 downto 0);
  signal sp2 : std_logic_vector(5 downto 0);
begin
  u : entity work.t_vhdl_localconstwidth_m generic map (Width_g => 8) port map (cw => "0000000000101", sp => sp);
  u2 : entity work.t_vhdl_localconstwidth_m generic map (Width_g => 16)
    port map (cw => "0000000000000000001101", sp => sp2);
  process
  begin
    wait for 1 ns;
    assert sp = "00010" and sp2 = "100001"
      report "sp " & to_string(sp) & " sp2 " & to_string(sp2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
