import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useAuth } from "@/contexts/AuthContext";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { toast } from "sonner";

const ForceAdminSetup = () => {
  const [loading, setLoading] = useState(false);
  const [adminKey, setAdminKey] = useState("");
  const { user } = useAuth();

  // Simple bypass - in production you'd want better security
  const ADMIN_SETUP_KEY = "portfolio-admin-2024";

  const forceAdminSetup = async () => {
    if (!user) {
      toast.error("You must be logged in");
      return;
    }

    if (adminKey !== ADMIN_SETUP_KEY) {
      toast.error("Invalid admin setup key");
      return;
    }

    setLoading(true);
    try {
      // Try to create the role directly
      const { error: insertError } = await supabase
        .from("user_roles")
        .upsert([
          {
            user_id: user.id,
            role: "admin"
          }
        ], { 
          onConflict: 'user_id,role',
          ignoreDuplicates: true 
        });

      if (insertError) {
        console.error("Insert error:", insertError);
        toast.error("Setup failed. Please use manual SQL method below.");
        return;
      }

      toast.success("Admin role setup complete! Refreshing page...");
      setTimeout(() => window.location.reload(), 1500);
      
    } catch (error) {
      console.error("Error:", error);
      toast.error("Setup failed. Please try manual method.");
    } finally {
      setLoading(false);
    }
  };

  return (
    <Card className="p-6">
      <h3 className="text-lg font-semibold mb-4">Force Admin Setup</h3>
      
      <div className="space-y-4">
        <div>
          <Label htmlFor="adminKey">Admin Setup Key</Label>
          <Input
            id="adminKey"
            type="password"
            value={adminKey}
            onChange={(e) => setAdminKey(e.target.value)}
            placeholder="Enter admin setup key"
          />
          <p className="text-xs text-muted-foreground mt-1">
            Key: <code>portfolio-admin-2024</code>
          </p>
        </div>

        <Button 
          onClick={forceAdminSetup}
          disabled={loading || !adminKey}
          className="w-full"
        >
          {loading ? "Setting up..." : "Force Admin Setup"}
        </Button>

        <div className="text-xs text-muted-foreground">
          <p><strong>Your User ID:</strong> {user?.id}</p>
          <p><strong>Manual SQL (if above fails):</strong></p>
          <div className="bg-muted p-2 rounded font-mono text-xs mt-1">
            INSERT INTO public.user_roles (user_id, role) VALUES ('{user?.id}', 'admin') ON CONFLICT DO NOTHING;
          </div>
        </div>
      </div>
    </Card>
  );
};

export default ForceAdminSetup;
