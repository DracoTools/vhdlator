-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package holder_a is
    procedure note (n : natural; got : out natural);
end package;

package body holder_a is
    function seed return natural is
    begin
        return 5;
    end function;

    -- Standing between two subprograms of a package body, which is where the
    -- question "is this the package's own object?" used to be answered by
    -- looking at the neighbour rather than at what encloses it
    constant width : natural := seed + 3;

    procedure note (n : natural; got : out natural) is
    begin
        got := n + width;
    end procedure;
end package body;

package holder_b is
    procedure note (n : natural; got : out natural);
end package;

package body holder_b is
    function seed return natural is
    begin
        return 100;
    end function;

    -- The same simple name as the constant holder_a declares, so the two have
    -- to be told apart wherever both end up
    constant width : natural := seed + 1;

    procedure note (n : natural; got : out natural) is
    begin
        got := n + width;
    end procedure;
end package body;

package tagging is
    function tag_value return natural;
end package;

package body tagging is
    -- Reads what only running settles, so it is not a function the back end
    -- may run to work out a constant
    impure function start_tag return natural is
    begin
        if now = 0 fs then return 7; end if;
        return 9;
    end function;

    constant first_tag : natural := start_tag;

    function tag_value return natural is
    begin
        return first_tag;
    end function;
end package body;

use work.holder_a;
use work.holder_b;
use work.tagging.all;

entity leaf is
    generic (base : natural);
    port (a : out natural; b : out natural; t : out natural);
end entity;

architecture rtl of leaf is
begin
    -- Each instance reads the same three package objects, of which the
    -- language says there is one apiece however many read them
    process is
        variable va, vb : natural;
    begin
        holder_a.note(base, va);
        holder_b.note(base, vb);
        a <= va;
        b <= vb;
        t <= tag_value;
        wait;
    end process;
end architecture;

use work.holder_a;

entity t_vhdl_pkgconst is
end entity;

architecture test of t_vhdl_pkgconst is
    signal a1, b1, t1 : natural;
    signal a2, b2, t2 : natural;
begin
    one : entity work.leaf generic map (base => 10) port map (a => a1, b => b1, t => t1);
    two : entity work.leaf generic map (base => 20) port map (a => a2, b => b2, t => t2);

    check : process is
    begin
        wait for 1 ns;
        assert a1 = 18 report "a1 = " & integer'image(a1) severity failure;
        assert b1 = 111 report "b1 = " & integer'image(b1) severity failure;
        assert a2 = 28 report "a2 = " & integer'image(a2) severity failure;
        assert b2 = 121 report "b2 = " & integer'image(b2) severity failure;
        assert t1 = 7 report "t1 = " & integer'image(t1) severity failure;
        assert t2 = 7 report "t2 = " & integer'image(t2) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
