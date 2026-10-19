-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- PoC's configuration packages, in small (GHDL gna bug035): a deferred table
-- of records whose strings a function pads with a fill character, searched
-- by functions while the package constants are elaborated (14.4.2), and a
-- scalar constant of what they find sizes a signal.

package t_vhdl_boardtable_priv is
  constant fill_c : character;
  subtype name_t is string(1 to 16);
  type board_t is record
    name : name_t;
    device : name_t;
    count : natural;
  end record;
  type board_vector is array (natural range <>) of board_t;
  constant boards : board_vector;
  function conf (s : string) return name_t;
  function trim (s : string) return string;
  function board_index (name : string) return natural;
  function is_xilinx (name : string) return boolean;
end package;

package body t_vhdl_boardtable_priv is
  constant fill_c : character := '~';
  function conf (s : string) return name_t is
    variable r : name_t;
  begin
    r := (others => fill_c);
    r(1 to s'length) := s;
    return r;
  end function;
  constant boards : board_vector := (
    (name => conf("DE0"), device => conf("EP3C16"), count => 0),
    (name => conf("ML505"), device => conf("XC5VLX50T"), count => 1),
    (name => conf("KC705"), device => conf("XC7K325T"), count => 2));
  function trim (s : string) return string is
  begin
    for i in s'range loop
      if s(i) = fill_c then
        return s(s'low to i - 1);
      end if;
    end loop;
    return s;
  end function;
  function board_index (name : string) return natural is
  begin
    for i in boards'range loop
      if trim(boards(i).name) = name then
        return i;
      end if;
    end loop;
    return 0;
  end function;
  function is_xilinx (name : string) return boolean is
    constant dev : name_t := boards(board_index(name)).device;
  begin
    return dev(1 to 2) = "XC";
  end function;
end package body;

use work.t_vhdl_boardtable_priv.all;

package t_vhdl_boardtable_cfg is
  constant my_board : string := "ML505";
  constant chosen : natural := board_index(my_board);
  constant xilinx : boolean := is_xilinx(my_board);
  constant chosen_count : natural := boards(board_index(my_board)).count;
end package;

use work.t_vhdl_boardtable_cfg.all;

entity t_vhdl_boardtable is
end entity;

architecture behav of t_vhdl_boardtable is
  signal s : bit_vector(0 to chosen);
  signal t : bit_vector(0 to chosen_count);
begin
  process begin
    assert s'length = 2 report "chosen " & integer'image(chosen) severity failure;
    assert t'length = 2 report "count " & integer'image(chosen_count) severity failure;
    assert xilinx severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
