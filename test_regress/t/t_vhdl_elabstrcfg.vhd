-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Widths decoded while elaborating from a string generic, the way VUnit's
-- testbenches read the configuration run.py encodes: split into an array of
-- LINE, stripped, looked up by a procedure that hands the value back through
-- an inout LINE, and read with 'VALUE (14.4.2). The widths size a signal and
-- an instance's generic.

library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;

package t_vhdl_elabstrcfg_pkg is
  type lines_t is array (natural range <>) of line;
  type lines_ptr is access lines_t;
  impure function split(s : string; sep : character) return lines_ptr;
  function strip(s : string) return string;
  type status_t is (valid_value, key_error);
  procedure lookup(d : string; key : string; variable value : inout line;
                   variable status : out status_t);
  impure function get(d : string; key : string) return string;
end package;

package body t_vhdl_elabstrcfg_pkg is
  impure function split(s : string; sep : character) return lines_ptr is
    variable n : natural := 1;
    variable result : lines_ptr;
    variable start, k : natural;
  begin
    for i in s'range loop
      if s(i) = sep then
        n := n + 1;
      end if;
    end loop;
    result := new lines_t(0 to n - 1);
    start := s'left;
    k := 0;
    for i in s'range loop
      if s(i) = sep then
        result(k) := new string'(s(start to i - 1));
        k := k + 1;
        start := i + 1;
      end if;
    end loop;
    result(k) := new string'(s(start to s'right));
    return result;
  end function;

  function strip(s : string) return string is
    variable first : natural := s'left;
    variable last : integer := s'right;
  begin
    while first <= s'right and s(first) = ' ' loop
      first := first + 1;
    end loop;
    while last >= first and s(last) = ' ' loop
      last := last - 1;
    end loop;
    return s(first to last);
  end function;

  -- As VUnit's dictionary does: a procedure hands the value back through an
  -- inout LINE and a status through an out parameter
  procedure lookup(d : string; key : string; variable value : inout line;
                   variable status : out status_t) is
    variable pairs, pair : lines_ptr;
  begin
    if value /= null then
      deallocate(value);
    end if;
    pairs := split(d, ',');
    for i in pairs'range loop
      pair := split(pairs(i).all, ':');
      if pair'length = 2 and strip(pair(0).all) = strip(key) then
        write(value, strip(pair(1).all));
        status := valid_value;
        return;
      end if;
    end loop;
    status := key_error;
  end procedure;

  impure function get(d : string; key : string) return string is
    variable value : line;
    variable status : status_t;
  begin
    lookup(d, key, value, status);
    if status = valid_value then
      return value.all;
    end if;
    return "";
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabstrcfg_pkg.all;

entity t_vhdl_elabstrcfg_sub is
  generic (width : positive);
  port (d : out std_logic_vector(width - 1 downto 0));
end entity;
architecture a of t_vhdl_elabstrcfg_sub is
begin
  d <= (others => '1');
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_elabstrcfg_pkg.all;

entity t_vhdl_elabstrcfg is
  generic (encoded_tb_cfg : string := "dat_width : 16, adr_width:8");
end entity;

architecture a of t_vhdl_elabstrcfg is
  type tb_cfg_t is record
    dat_width : positive;
    adr_width : positive;
  end record;
  impure function decode(s : string) return tb_cfg_t is
  begin
    return (dat_width => positive'value(get(s, "dat_width")),
            adr_width => positive'value(get(s, "adr_width")));
  end function;
  constant tb_cfg : tb_cfg_t := decode(encoded_tb_cfg);
  signal dat : std_logic_vector(tb_cfg.dat_width - 1 downto 0);
  signal adr : std_logic_vector(tb_cfg.adr_width - 1 downto 0);
begin
  i_sub : entity work.t_vhdl_elabstrcfg_sub generic map (width => tb_cfg.adr_width) port map (d => adr);
  process
  begin
    wait for 1 ns;
    assert dat'length = 16 and adr'length = 8
      report "dat'length=" & integer'image(dat'length) & " adr'length=" & integer'image(adr'length)
      severity failure;
    assert adr = x"ff" report "adr=" & to_hstring(adr) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
