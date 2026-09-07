-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant initialised by an impure function is still a globally static
-- primary, even though the expression that made it is not (9.4.3, note 2),
-- so it may be handed to an instance as a generic.  That is how every one
-- of VUnit's verification components is given its handle.  A generic whose
-- value does fold has to go on being a parameter, since that is what makes
-- one copy of a module per set of values.

package t_vhdl_genhandle_pkg is
    type handle_t is record
        id  : integer;
        len : natural;
    end record;
    impure function new_handle (n : natural) return handle_t;
end package;

package body t_vhdl_genhandle_pkg is
    impure function new_handle (n : natural) return handle_t is
    begin
        return (id => 7, len => n);
    end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;

-- A generic whose value only elaboration decides, beside one that folds and
-- so must still size things
entity t_vhdl_genhandle_child is
    generic (h     : work.t_vhdl_genhandle_pkg.handle_t;
             width : natural := 4;
             tag   : integer := 99);
    port (o : out integer;
          v : out std_logic_vector(width-1 downto 0));
end entity;

architecture a of t_vhdl_genhandle_child is
begin
    -- read before anything has run, which is when a generic is certainly there
    process is
    begin
        assert h.len = 8 report "handle not there at time zero" severity failure;
        assert h.id = 7 report "handle id at time zero" severity failure;
        assert tag = 99 report "default generic at time zero" severity failure;
        wait;
    end process;

    o <= h.len + h.id;
    v <= (others => '1');
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_genhandle_pkg.all;

entity t_vhdl_genhandle is
end entity;

architecture a of t_vhdl_genhandle is
    constant h1 : handle_t := new_handle(8);
    signal s1   : integer;
    signal v1   : std_logic_vector(5 downto 0);
begin

    -- the handle is not foldable; the width beside it is, and still sizes a port
    dut : entity work.t_vhdl_genhandle_child
        generic map (h => h1, width => 6)
        port map (o => s1, v => v1);

    p: process is
    begin
        wait for 1 ns;
        assert s1 = 15 report "handle through a generic" severity failure;
        assert v1'length = 6 report "a folding generic still sizes a port" severity failure;
        assert v1 = "111111" report "sized port value" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;

end architecture;
