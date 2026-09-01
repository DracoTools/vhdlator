-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_sigrecgen_pkg is
    -- A bus interface whose widths the instance settles, which is how a
    -- verification component takes the design it drives (5.3.3)
    type t_if is record
        cs   : std_logic;
        addr : unsigned;
        data : std_logic_vector;
    end record;

    procedure write_one (signal cs   : inout std_logic;
                         signal addr : inout unsigned;
                         signal data : inout std_logic_vector;
                         constant v  : in unsigned(3 downto 0));
    procedure write_if (signal iface : inout t_if;
                        constant v : in unsigned(3 downto 0));
end package;

package body t_vhdl_sigrecgen_pkg is
    procedure write_one (signal cs   : inout std_logic;
                         signal addr : inout unsigned;
                         signal data : inout std_logic_vector;
                         constant v  : in unsigned(3 downto 0)) is
        -- How long the signal the call passed is, which only the object at
        -- the end of the chain says: here an element of a record that a
        -- port constrains with a generic (4.2.2.3)
        variable norm : unsigned(addr'length - 1 downto 0);
    begin
        assert norm'length = 8 report "norm length" severity failure;
        assert data'length = 16 report "data length" severity failure;
        norm                        := (others => '0');
        norm(v'length - 1 downto 0) := v;
        cs                          <= '1';
        addr                        <= norm;
        data                        <= (data'range => '0');
    end procedure;

    procedure write_if (signal iface : inout t_if;
                        constant v : in unsigned(3 downto 0)) is
    begin
        write_one(iface.cs, iface.addr, iface.data, v);
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_sigrecgen_pkg.all;

entity t_vhdl_sigrecgen_vvc is
    generic (GC_ADDR_WIDTH : natural := 8;
             GC_DATA_WIDTH : natural := 16);
    port (iface : inout t_if(addr(GC_ADDR_WIDTH - 1 downto 0),
                             data(GC_DATA_WIDTH - 1 downto 0)));
end entity;

architecture test of t_vhdl_sigrecgen_vvc is
begin
    main : process is
    begin
        wait for 1 ns;
        write_if(iface, to_unsigned(5, 4));
        wait;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_sigrecgen_pkg.all;

entity t_vhdl_sigrecgen is
end entity;

architecture test of t_vhdl_sigrecgen is
    signal iface : t_if(addr(7 downto 0), data(15 downto 0));
begin

    vvc : entity work.t_vhdl_sigrecgen_vvc
        generic map (GC_ADDR_WIDTH => 8, GC_DATA_WIDTH => 16)
        port map (iface => iface);

    check : process is
    begin
        wait for 2 ns;
        assert iface.cs = '1' report "cs" severity failure;
        assert iface.addr = 5 report "addr " & to_string(iface.addr) severity failure;
        assert iface.addr'length = 8 report "addr length" severity failure;
        assert iface.data = x"0000" report "data " & to_string(iface.data) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
