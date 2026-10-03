-- DESCRIPTION: Verilator: VHDL value held two levels deep copied into a
-- parameter whose element length names a generic
--
-- A procedure fills a local array of vectors, as long as its parameter and
-- with elements as long as the parameter's, and hands it back through the
-- parameter (10.6.2.1). The caller's actual is "t_slv_array(2 to 6)(G - 1
-- downto 0)", so the copy of the procedure made for the call holds the
-- parameter at a length the instance settles -- UVVM's avalon_st_receive
-- ending with "data_array := v_normalized_data" when its testbench's widths
-- are written as numbers.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_dyn2genout_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  procedure fill(variable data_array : out t_slv_array; constant first : natural);
end package;
package body t_vhdl_dyn2genout_pkg is
  procedure fill(variable data_array : out t_slv_array; constant first : natural) is
    constant C_W : natural := data_array(data_array'low)'length;
    variable v : t_slv_array(0 to data_array'length - 1)(C_W - 1 downto 0);
  begin
    for i in v'range loop
      for j in 0 to C_W - 1 loop
        v(i)(j) := '1' when ((first + i) / (2 ** j)) mod 2 = 1 else '0';
      end loop;
    end loop;
    data_array := v;
  end procedure;
end package body;
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_dyn2genout_pkg.all;
entity t_vhdl_dyn2genout is
  generic (G : natural := 8);
end entity;
architecture a of t_vhdl_dyn2genout is
begin
  process is
    variable d : t_slv_array(2 to 6)(G - 1 downto 0);
  begin
    fill(d, 10);
    for i in d'range loop
      report integer'image(i) & " " & integer'image(to_integer(unsigned(d(i))));
      assert to_integer(unsigned(d(i))) = 8 + i severity failure;
    end loop;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
