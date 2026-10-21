-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A parameter carried with its own bounds -- its default a literal -- is
-- written into a slice of a variable by the parameter's own range,
-- "oh_v(onehot'range) := onehot": the ends are the actual's, known only as
-- the procedure runs, so the elements go in between them then. The slice
-- was emitted as a part select with those ends, which are no constants.
-- Shaped after open-logic's olo_base_cam_tb readCam.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_dynrunslice is
  generic (addresses_g : positive := 4);
end entity;
architecture a of t_vhdl_dynrunslice is
  procedure readCam(variable seen : inout std.textio.line; addr : integer := 0;
                    found : boolean := true; onehot : std_logic_vector := "X";
                    msg : string := "") is
    variable oh_v : std_logic_vector(addresses_g - 1 downto 0) := (others => '0');
  begin
    if found then
      if onehot = "X" then
        oh_v(addr) := '1';
      else
        oh_v(onehot'range) := onehot;
      end if;
    end if;
    std.textio.write(seen, msg & to_string(oh_v) & " ");
  end procedure;
begin
  process
    variable oh : std_logic_vector(addresses_g - 1 downto 0);
    variable seen : std.textio.line;
  begin
    readCam(seen, addr => 2, msg => "a");
    oh := (others => '0');
    oh(2 downto 1) := "11";
    readCam(seen, addr => 0, onehot => oh, msg => "b");
    readCam(seen, addr => 0, onehot => oh(3 downto 2), msg => "c");
    readCam(seen, addr => 0, found => false, msg => "d");
    assert seen.all = "a0100 b0110 c0100 d0000 " report "seen " & seen.all severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
