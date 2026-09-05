-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_protopen_pkg is
    type holder_t is protected
        procedure init (data : std_logic_vector);
        impure function width return natural;
    end protected;
end package;

package body t_vhdl_protopen_pkg is
    type holder_t is protected body
        variable p_width : natural := 0;
        procedure init (data : std_logic_vector) is
        begin
            p_width := data'length;
        end procedure;
        impure function width return natural is
        begin
            return p_width;
        end function;
    end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_protopen_slave is
    port (
        rdata : out std_logic_vector;
        got : out natural
    );
end entity;

library work;
use work.t_vhdl_protopen_pkg.all;

architecture a of t_vhdl_protopen_slave is
    -- One object of a protected type, shared however many instances the
    -- design elaborates -- so a copy of its method made for one instance's
    -- port would name generics belonging to that instance alone
    shared variable self : holder_t;
begin
    process
    begin
        self.init(rdata);
        got <= self.width;
        wait;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_protopen is
end entity;

architecture a of t_vhdl_protopen is
    signal narrow : std_logic_vector(7 downto 0);
    signal wide : std_logic_vector(15 downto 0);
    signal gn : natural;
    signal gw : natural;
begin
    u_narrow : entity work.t_vhdl_protopen_slave
        port map (rdata => narrow, got => gn);
    u_wide : entity work.t_vhdl_protopen_slave
        port map (rdata => wide, got => gw);

    process
    begin
        wait for 1 ns;
        assert gn = 8 report "narrow got " & integer'image(gn) severity failure;
        assert gw = 16 report "wide got " & integer'image(gw) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
