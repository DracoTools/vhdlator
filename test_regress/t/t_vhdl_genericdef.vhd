-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_genericdef_pkg is
    type t_config is record
        width   : natural;
        enabled : boolean;
    end record;

    -- A verification component's configuration arrives as the default of a
    -- generic, so what that default names is settled by elaboration too
    constant C_CONFIG_DEFAULT : t_config := (width => 5, enabled => true);
end package;

use work.t_vhdl_genericdef_pkg.all;

entity t_vhdl_genericdef_sub is
    generic (GC_CONFIG : t_config := C_CONFIG_DEFAULT);
    port (o : out natural);
end entity;

architecture test of t_vhdl_genericdef_sub is
begin
    o <= GC_CONFIG.width when GC_CONFIG.enabled else 0;
end architecture;

use work.t_vhdl_genericdef_pkg.all;

entity t_vhdl_genericdef is
end entity;

architecture test of t_vhdl_genericdef is
    signal from_default : natural;
    signal from_given   : natural;
begin

    u_default : entity work.t_vhdl_genericdef_sub
        port map (o => from_default);

    u_given : entity work.t_vhdl_genericdef_sub
        generic map (GC_CONFIG => (width => 9, enabled => true))
        port map (o => from_given);

    check : process is
    begin
        wait for 1 ns;
        assert from_default = 5
            report "default " & integer'image(from_default) severity failure;
        assert from_given = 9
            report "given " & integer'image(from_given) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
