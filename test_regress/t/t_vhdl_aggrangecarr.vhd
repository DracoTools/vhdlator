-- DESCRIPTION: Verilator: VHDL aggregate over a formal's range, handed on
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- "std_logic_vector'(d'range => v)", where d is a signal formal given a
-- signal a generic sizes, handed to a function whose parameter is
-- unconstrained -- VUnit's protocol checker test hands one to
-- to_nibble_string. The aggregate's length is not a number while
-- compiling, and it is handed over with the bounds it has when it runs.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_aggrangecarr_pkg is
  function show(constant value : unsigned) return string;
  function show(constant value : std_logic_vector) return string;
end package;

package body t_vhdl_aggrangecarr_pkg is
  function show(constant value : unsigned) return string is
    constant value_i : unsigned(value'length downto 1) := value;
    variable ret : string(1 to value'length);
  begin
    for i in value_i'range loop
      ret(value'length - i + 1) := std_logic'image(value_i(i))(2);
    end loop;
    return ret;
  end;

  function show(constant value : std_logic_vector) return string is
  begin
    return show(unsigned(value));
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aggrangecarr_pkg.all;

entity t_vhdl_aggrangecarr is
  generic (dl : natural := 6);
end entity;

architecture a of t_vhdl_aggrangecarr is
  signal clk : std_logic := '0';
  signal wide : std_logic_vector(dl - 1 downto 0) := (others => '0');
  signal narrow : std_logic_vector(0 to dl - 4) := (others => '0');
  constant values : std_logic_vector(1 to 2) := "10";
begin
  clk <= not clk after 5 ns;

  process
    procedure try(signal d : out std_logic_vector; constant len : natural) is
      variable expect : string(1 to len);
    begin
      for i in values'range loop
        d <= (d'range => values(i));
        wait until rising_edge(clk);
        expect := (others => std_logic'image(values(i))(2));
        report "got " & show(std_logic_vector'(d'range => values(i)));
        assert show(std_logic_vector'(d'range => values(i))) = expect
          report "shown" severity failure;
        assert show(std_logic_vector'(d'reverse_range => values(i)))'length = len
          report "reversed" severity failure;
      end loop;
    end;
  begin
    try(wide, 6);
    try(narrow, 3);
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
