-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_rtslicearg_pkg is
    -- A parameter the body writes is handed the caller's own object, and a
    -- randomiser is handed the part of a word it is to fill (4.2.2.3)
    procedure fill (variable v : inout std_logic_vector; constant c : in std_logic);
end package;

package body t_vhdl_rtslicearg_pkg is
    procedure fill (variable v : inout std_logic_vector; constant c : in std_logic) is
    begin
        for i in v'range loop
            v(i) := c;
        end loop;
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_rtslicearg_pkg.all;

entity t_vhdl_rtslicearg is
end entity;

architecture test of t_vhdl_rtslicearg is
begin

    main : process is
        variable w : std_logic_vector(7 downto 0) := (others => '0');
        variable n : integer := 4;
    begin
        -- The ends of the slice are worked out while running, so how long
        -- the value is is not known until the call is made (8.5)
        fill(w(n - 1 downto 0), '1');
        assert w = "00001111" report "low " & to_string(w) severity failure;

        n := 6;
        fill(w(n - 1 downto 2), '0');
        assert w = "00000011" report "middle " & to_string(w) severity failure;

        -- What was written comes back into the slice it was taken from, and
        -- nothing outside it moves
        fill(w(7 downto n), '1');
        assert w = "11000011" report "high " & to_string(w) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
