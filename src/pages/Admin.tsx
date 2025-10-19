import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useAuth } from "@/contexts/AuthContext";
import { LogOut, Mail, FolderOpen, FileText, User, MessageSquare } from "lucide-react";
import ContactsManagement from "@/components/admin/ContactsManagement";
import ProjectsManagement from "@/components/admin/ProjectsManagement";
import EnhancedBlogManagement from "@/components/admin/EnhancedBlogManagement";
import ResumeManagement from "@/components/admin/ResumeManagement";
import TestimonialsManagement from "@/components/admin/TestimonialsManagement";
import AdminSetup from "@/components/admin/AdminSetup";

const Admin = () => {
  const { signOut, user, isAdmin, loading } = useAuth();
  const navigate = useNavigate();

  const handleSignOut = async () => {
    await signOut();
    navigate("/auth");
  };

  if (loading) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-center">
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-primary mx-auto mb-4"></div>
          <p>Loading...</p>
        </div>
      </div>
    );
  }

  if (!isAdmin) {
    return (
      <div className="min-h-screen py-12 px-4">
        <div className="container mx-auto max-w-2xl">
          <div className="flex justify-between items-center mb-8">
            <div>
              <h1 className="text-4xl font-bold mb-2">
                Admin <span className="text-gradient">Setup</span>
              </h1>
              <p className="text-muted-foreground">Welcome, {user?.email}</p>
            </div>
            <Button variant="outline" onClick={handleSignOut}>
              <LogOut className="mr-2 h-4 w-4" />
              Sign Out
            </Button>
          </div>
          <AdminSetup />
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen py-12 px-4">
      <div className="container mx-auto">
        <div className="flex justify-between items-center mb-8">
          <div>
            <h1 className="text-4xl font-bold mb-2">
              Admin <span className="text-gradient">Dashboard</span>
            </h1>
            <p className="text-muted-foreground">Welcome back, {user?.email}</p>
          </div>
          <Button variant="outline" onClick={handleSignOut}>
            <LogOut className="mr-2 h-4 w-4" />
            Sign Out
          </Button>
        </div>

        <Tabs defaultValue="contacts" className="w-full">
          <TabsList className="grid w-full max-w-3xl grid-cols-5 mb-8">
            <TabsTrigger value="contacts">
              <Mail className="mr-2 h-4 w-4" />
              Contacts
            </TabsTrigger>
            <TabsTrigger value="projects">
              <FolderOpen className="mr-2 h-4 w-4" />
              Projects
            </TabsTrigger>
            <TabsTrigger value="blog">
              <FileText className="mr-2 h-4 w-4" />
              Blog
            </TabsTrigger>
            <TabsTrigger value="testimonials">
              <MessageSquare className="mr-2 h-4 w-4" />
              Testimonials
            </TabsTrigger>
            <TabsTrigger value="resume">
              <User className="mr-2 h-4 w-4" />
              Resume
            </TabsTrigger>
          </TabsList>
          
          <TabsContent value="contacts">
            <ContactsManagement />
          </TabsContent>
          
          <TabsContent value="projects">
            <ProjectsManagement />
          </TabsContent>
          
          <TabsContent value="blog">
            <EnhancedBlogManagement />
          </TabsContent>
          
          <TabsContent value="testimonials">
            <TestimonialsManagement />
          </TabsContent>
          
          <TabsContent value="resume">
            <ResumeManagement />
          </TabsContent>
        </Tabs>
      </div>
    </div>
  );
};

export default Admin;
