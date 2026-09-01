-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_portdrive_pkg is
    -- A bus functional model takes the signals it drives as parameters, so
    -- the drive is written inside the procedure and the caller's own signal
    -- is what gets a driver (4.2.2.3)
    procedure drive (signal addr : inout unsigned; constant v : in unsigned);
end package;

package body t_vhdl_portdrive_pkg is
    procedure drive (signal addr : inout unsigned; constant v : in unsigned) is
        variable norm : unsigned(addr'length - 1 downto 0) := resize(v, addr'length);
    begin
        addr <= norm;
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_portdrive_pkg.all;

entity t_vhdl_portdrive_sub is
    port (addr : inout unsigned(7 downto 0);
          flag : inout std_logic);
end entity;

architecture test of t_vhdl_portdrive_sub is
begin

    main : process is
        -- One declared inside the process drives a port it can see without
        -- being handed it, which is where the LRM allows that (10.5.2.1)
        procedure raise_flag is
        begin
            flag <= '1';
        end procedure;
    begin
        wait for 1 ns;
        drive(addr, to_unsigned(5, 4));
        raise_flag;
        wait for 2 ns;
        drive(addr, to_unsigned(200, 8));
        wait;
    end process;

end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_portdrive is
end entity;

architecture test of t_vhdl_portdrive is
    signal addr : unsigned(7 downto 0);
    signal flag : std_logic;
begin

    dut : entity work.t_vhdl_portdrive_sub port map (addr => addr, flag => flag);

    check : process is
    begin
        wait for 2 ns;
        assert addr = 5 report "addr " & to_string(addr) severity failure;
        assert flag = '1' report "flag" severity failure;
        wait for 2 ns;
        assert addr = 200 report "addr again " & to_string(addr) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
